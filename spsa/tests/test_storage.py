import csv
import json
import io
import os
from contextlib import redirect_stdout
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from spsa.config import catalog, digest
from spsa.storage import (RunLock, begin_attempt, commit_iteration, commit_pair, connect,
                          create_run, ensure_iteration, export_csv, meta, verify)


class StorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("engine.exe", "cutechess.exe"):
            (self.root / name).write_bytes(b"test executable snapshot")
        (self.root / "book.epd").write_text("4k3/8/8/8/8/8/P7/4K3 w - - 0 1\n")
        params = {n: dict(p, start=p["default"], min=p["engine_min"], max=p["engine_max"], c_end=1., r_end=.1)
                  for n, p in catalog().items()}
        self.cfg = dict(version=1, profile="test", selected=["AspirationDelta"], parameters=params,
                        paths=dict(engine=str(self.root / "engine.exe"), cutechess=str(self.root / "cutechess.exe"),
                                   openings=str(self.root / "book.epd"), runs=str(self.root / "runs")),
                        algorithm=dict(iterations=4, seed=123, alpha=.602, gamma=.101, A_ratio=.1),
                        match=dict(pairs_per_iteration=2))

    def new(self):
        folder = create_run(self.cfg, [])
        return folder, connect(folder)

    def complete_pairs(self, db, folder, k):
        for pair_id in range(2):
            row = db.execute("SELECT result FROM pairs WHERE k=? AND pair_id=?", (k, pair_id)).fetchone()
            if row[0]:
                continue
            attempt, path = begin_attempt(db, folder, k, pair_id)
            scores = [1, 1] if pair_id == 0 else [-1, 0]
            result = dict(score=sum(scores), games=[dict(game=i+1, score=s) for i, s in enumerate(scores)],
                          elapsed=1, artifacts=str(path))
            commit_pair(db, k, pair_id, attempt, result)

    def test_pending_schedule_and_completed_pairs_survive(self):
        folder, db = self.new()
        original = meta(db, "rng")
        step = ensure_iteration(db, folder)
        specs = list(db.execute("SELECT spec FROM pairs ORDER BY pair_id"))
        self.complete_pairs(db, folder, 1)
        db.close()
        db = connect(folder)
        try:
            self.assertEqual(ensure_iteration(db, folder), step)
            self.assertEqual([r[0] for r in db.execute("SELECT spec FROM pairs ORDER BY pair_id")], [r[0] for r in specs])
            self.assertEqual(meta(db, "rng"), original)
            self.assertTrue(commit_iteration(db, 1))
            self.assertFalse(commit_iteration(db, 1))
            self.assertNotEqual(meta(db, "rng"), original)
        finally:
            db.close()

    def test_transaction_rollback_and_killed_writer(self):
        folder, db = self.new()
        ensure_iteration(db, folder)
        self.complete_pairs(db, folder, 1)
        original = meta(db, "theta")
        def fail(_):
            raise RuntimeError("injected power failure")
        with self.assertRaises(RuntimeError):
            commit_iteration(db, 1, fail)
        self.assertEqual(meta(db, "theta"), original)
        self.assertEqual(meta(db, "iteration"), 0)
        db.close()
        script = "import os,sys; from spsa.storage import connect,commit_iteration; db=connect(sys.argv[1]); commit_iteration(db,1,lambda _: os._exit(71))"
        killed = subprocess.run([sys.executable, "-c", script, str(folder)], capture_output=True)
        self.assertEqual(killed.returncode, 71, killed.stderr)
        db = connect(folder)
        try:
            self.assertEqual(meta(db, "theta"), original)
            self.assertEqual(meta(db, "iteration"), 0)
            commit_iteration(db, 1)
            self.assertEqual(meta(db, "iteration"), 1)
        finally:
            db.close()

    def test_resumed_matches_uninterrupted_optimizer_and_csv(self):
        runs = []
        for interrupted in (False, True):
            folder, db = self.new()
            for k in range(1, 5):
                ensure_iteration(db, folder)
                if interrupted:
                    db.close(); db = connect(folder)
                    ensure_iteration(db, folder)
                self.complete_pairs(db, folder, k)
                if interrupted:
                    db.close(); db = connect(folder)
                commit_iteration(db, k)
                if interrupted:
                    db.close(); db = connect(folder)
            export_csv(db, folder)
            runs.append((meta(db, "theta"), meta(db, "rng"), (folder / "parameters.csv").read_text(),
                         (folder / "final_parameters.csv").read_text()))
            db.close()
        self.assertEqual(runs[0], runs[1])

    def test_duplicate_and_incomplete_updates(self):
        folder, db = self.new()
        try:
            ensure_iteration(db, folder)
            with self.assertRaises(ValueError): commit_iteration(db, 1)
            attempt, path = begin_attempt(db, folder, 1, 0)
            result = dict(score=2, games=[dict(score=1), dict(score=1)], elapsed=1, artifacts=str(path))
            commit_pair(db, 1, 0, attempt, result)
            commit_pair(db, 1, 0, attempt, result)
            with self.assertRaises(ValueError):
                commit_pair(db, 1, 0, attempt, dict(result, elapsed=2))
            with self.assertRaises(ValueError):
                commit_pair(db, 1, 1, attempt, result)
        finally:
            db.close()

    def test_snapshot_verification_and_csv_repair(self):
        folder, db = self.new()
        try:
            verify(folder, db)
            export_csv(db, folder)
            (folder / "games.csv").write_text("truncated")
            export_csv(db, folder)
            marker = json.loads((folder / "export.json").read_text())
            self.assertEqual(marker["files"]["games.csv"], digest(folder / "games.csv"))
            (folder / "assets/Tunguska.exe").write_bytes(b"different executable")
            with self.assertRaises(ValueError): verify(folder, db)
        finally:
            db.close()

    def test_exclusive_lock(self):
        folder, db = self.new()
        db.close()
        with RunLock(folder):
            script = "import sys; from spsa.storage import RunLock; lock=RunLock(sys.argv[1]); lock.__enter__()"
            result = subprocess.run([sys.executable, "-c", script, str(folder)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"Another process", result.stderr)
        with RunLock(folder):
            pass

    def test_cli_status_and_export_accept_relative_run_paths(self):
        from spsa.__main__ import main
        folder, db = self.new()
        db.close()
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            relative = str(folder.relative_to(self.root))
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["status", "--run", relative]), 0)
                self.assertEqual(main(["export", "--run", relative]), 0)
            self.assertIn('"status": "ready"', output.getvalue())
            self.assertTrue((folder / "export.json").is_file())
        finally:
            os.chdir(previous)
