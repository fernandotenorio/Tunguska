"""Real cutechess smoke test: kill the coordinator mid-iteration, then resume."""
import argparse
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
import uuid

from spsa.config import load
from spsa.storage import connect, meta


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path("spsa/config.toml"))
    args = parser.parse_args()
    cfg = load(args.config, "search", 2)
    workspace = Path("spsa/build") / ("integration-" + uuid.uuid4().hex[:8])
    workspace = workspace.resolve()
    workspace.mkdir(parents=True)
    runs = workspace / "runs"
    text = args.config.read_text(encoding="utf-8")
    start, end = text.index("[paths]"), text.index("[match]")
    paths = dict(cfg["paths"], runs=str(runs))
    text = text[:start] + "[paths]\n" + "\n".join(f"{key} = {json.dumps(value)}" for key, value in paths.items()) + "\n\n" + text[end:]
    text = text.replace('tc = "10+0.1"', 'tc = "1+0.05"').replace("depth = 0", "depth = 6")
    text = text.replace("max_moves = 0", "max_moves = 30").replace("pairs_per_iteration = 8", "pairs_per_iteration = 4")
    text = text.replace("concurrency = 1", "concurrency = 2")
    config_path = workspace / "smoke.toml"
    config_path.write_text(text, encoding="utf-8")
    with (workspace / "initial.log").open("w") as log:
        proc = subprocess.Popen([sys.executable, "-m", "spsa", "run", "--config", str(config_path), "--iterations", "2"], stdout=log, stderr=subprocess.STDOUT)
        retained = {}
        folder = None
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    raise RuntimeError("Initial run ended before interruption; inspect " + str(workspace))
                databases = list(runs.glob("*/state.sqlite3"))
                if databases:
                    folder = databases[0].parent
                    db = connect(folder, readonly=True)
                    try:
                        retained = {(r["k"], r["pair_id"]): r["result"] for r in db.execute("SELECT * FROM pairs WHERE result IS NOT NULL")}
                    except sqlite3.OperationalError:
                        pass  # Initialization transaction has not been published yet.
                    finally:
                        db.close()
                    if retained:
                        break
                time.sleep(.05)
            if not retained:
                raise RuntimeError("No committed pair before smoke-test deadline")
        finally:
            if proc.poll() is None:
                proc.kill()  # Deliberately bypass signal handlers and cleanup.
            proc.wait(timeout=10)
    with (workspace / "resumed.log").open("w") as log:
        result = subprocess.run([sys.executable, "-m", "spsa", "resume", "--run", str(folder)], stdout=log, stderr=subprocess.STDOUT, timeout=120)
    if result.returncode:
        raise RuntimeError("Resume failed; inspect " + str(workspace))
    db = connect(folder, readonly=True)
    try:
        assert meta(db, "iteration") == 2
        assert meta(db, "status") == "complete"
        assert db.execute("SELECT COUNT(*) FROM pairs WHERE result IS NOT NULL").fetchone()[0] == 8
        for identity, original in retained.items():
            assert db.execute("SELECT result FROM pairs WHERE k=? AND pair_id=?", identity).fetchone()[0] == original
            assert db.execute("SELECT COUNT(*) FROM attempts WHERE k=? AND pair_id=?", identity).fetchone()[0] == 1
        print(json.dumps(dict(result="passed", retained_pairs=len(retained), total_pairs=8, run=str(folder)), indent=2))
    finally:
        db.close()


if __name__ == "__main__":
    main()
