"""Local SPSA commands: validate, run, resume, status, export."""
import argparse
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
import json
from pathlib import Path
import signal
import sqlite3
import sys
import threading

from .config import load
from .runner import play_pair, probe, probe_cutechess
from .storage import (RunLock, begin_attempt, commit_iteration, commit_pair, connect,
                      create_run, encode, ensure_iteration, export_csv, meta, set_meta, utc, verify)


def status(db):
    cfg = meta(db, "cfg")
    counts = db.execute("SELECT COUNT(*), SUM(result IS NOT NULL) FROM pairs").fetchone()
    return dict(status=meta(db, "status"), iterations=meta(db, "iteration"),
                budget=cfg["algorithm"]["iterations"], scheduled_pairs=counts[0],
                completed_pairs=counts[1] or 0,
                last_error=json.loads(db.execute("SELECT value FROM metadata WHERE name='last_error'").fetchone()[0])
                if db.execute("SELECT 1 FROM metadata WHERE name='last_error'").fetchone() else None)


def execute(folder, retry_failed=False):
    folder = Path(folder).resolve()
    with RunLock(folder):
        db = connect(folder)
        cancel = threading.Event()
        previous = {}
        try:
            cfg = verify(folder, db)
            prior_status = meta(db, "status")
            if prior_status == "failed" and not retry_failed:
                raise ValueError("Run paused on an error. Inspect logs, fix the cause, then resume --retry-failed")
            if meta(db, "iteration") >= cfg["algorithm"]["iterations"]:
                export_csv(db, folder)
                print("Run already complete.")
                return 0
            probe(cfg)
            for sig in (signal.SIGINT, signal.SIGTERM):
                previous[sig] = signal.signal(sig, lambda *_: cancel.set())
            with db:
                set_meta(db, "status", "running")
            export_csv(db, folder)
            while not cancel.is_set():
                step = ensure_iteration(db, folder)
                if step is None:
                    break
                k = step["k"]
                pending = db.execute("SELECT pair_id,spec FROM pairs WHERE k=? AND result IS NULL ORDER BY pair_id", (k,)).fetchall()
                failures = []
                with ThreadPoolExecutor(max_workers=cfg["match"]["concurrency"]) as pool:
                    jobs = {}
                    for row in pending:
                        attempt_id, attempt_folder = begin_attempt(db, folder, k, row["pair_id"])
                        future = pool.submit(play_pair, cfg, step, json.loads(row["spec"]), attempt_folder, cancel)
                        jobs[future] = (row["pair_id"], attempt_id)
                    while jobs:
                        done, _ = wait(jobs, timeout=0.2, return_when=FIRST_COMPLETED)
                        for future in done:
                            pair_id, attempt_id = jobs.pop(future)
                            try:
                                result = future.result()
                                commit_pair(db, k, pair_id, attempt_id, result)
                                print(f"iteration {k}: pair {pair_id + 1} committed, score {result['score']:+d}", flush=True)
                            except Exception as exc:
                                with db:
                                    db.execute("UPDATE attempts SET finished=?,error=? WHERE id=?", (utc(), str(exc), attempt_id))
                                if not isinstance(exc, InterruptedError):
                                    failures.append(str(exc))
                                cancel.set()
                if failures:
                    raise RuntimeError("; ".join(failures))
                complete = db.execute("SELECT COUNT(*) FROM pairs WHERE k=? AND result IS NULL", (k,)).fetchone()[0] == 0
                if complete:
                    commit_iteration(db, k)
                    export_csv(db, folder)
                    print(f"iteration {k}/{cfg['algorithm']['iterations']} committed", flush=True)
                if cancel.is_set():
                    break
            finished = meta(db, "iteration") == cfg["algorithm"]["iterations"]
            with db:
                set_meta(db, "status", "complete" if finished else "interrupted")
            export_csv(db, folder)
            print("Complete." if finished else "Stopped safely. Resume this run folder.", flush=True)
            return 0 if finished else 130
        except BaseException as exc:
            cancel.set()
            # A failed compatibility/preflight check must not rewrite run state.
            if previous:
                with db:
                    set_meta(db, "status", "interrupted" if isinstance(exc, KeyboardInterrupt) else "failed")
                    set_meta(db, "last_error", str(exc))
                try:
                    export_csv(db, folder)
                except OSError:
                    pass  # Database remains authoritative if disk/export is unavailable.
            raise
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
            db.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("validate", "run"):
        p = commands.add_parser(name)
        p.add_argument("--config", type=Path, default=Path(__file__).with_name("config.toml"))
        p.add_argument("--profile", default="search")
        p.add_argument("--iterations", type=int)
    for name in ("resume", "status", "export"):
        p = commands.add_parser(name)
        p.add_argument("--run", type=Path, required=True)
        if name == "resume":
            p.add_argument("--retry-failed", action="store_true", help="Explicitly retry an inspected infrastructure/protocol failure")
    args = parser.parse_args(argv)
    try:
        if args.command in ("run", "validate"):
            cfg = load(args.config, args.profile, args.iterations)
            if args.command == "run" and not cfg["algorithm"]["iterations"]:
                raise ValueError("Set --iterations N or a positive config iteration budget")
            runner_version = probe_cutechess(cfg)
            handshake = probe(cfg)
            if args.command == "validate":
                print(f"Valid: {len(cfg['parameters'])} catalog entries, {len(cfg['selected'])} selected; UCI acknowledged all settings.")
                if not cfg["algorithm"]["iterations"]:
                    print("Choose --iterations before running.")
                return 0
            folder = create_run(cfg, handshake, runner_version)
            print(f"Run folder: {folder}", flush=True)
            return execute(folder)
        if args.command == "resume":
            return execute(args.run, args.retry_failed)
        if args.command == "export":
            with RunLock(args.run):
                db = connect(args.run, readonly=True)
                try:
                    export_csv(db, args.run)
                finally:
                    db.close()
            print(f"Exported: {args.run.resolve()}")
            return 0
        db = connect(args.run, readonly=True)
        try:
            print(json.dumps(status(db), indent=2))
        finally:
            db.close()
        return 0
    except (ValueError, KeyError, OSError, RuntimeError, sqlite3.Error) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted. Resume using the run folder.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
