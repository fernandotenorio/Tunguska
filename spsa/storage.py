"""Durable local run journal. CSV is an export, never optimizer authority."""
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import random
import shutil
import sqlite3
import struct
import uuid

from .config import ROOT, code_digest, digest
from .optimizer import nearest, prepare, restore_random, update
from .runner import fen_identity


def utc():
    return datetime.now(timezone.utc).isoformat()


def encode(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


class RunLock:
    def __init__(self, folder):
        self.path = Path(folder) / "run.lock"
        self.file = None

    def __enter__(self):
        self.file = self.path.open("a+b")
        self.file.seek(0, 2)
        if self.file.tell() == 0:
            self.file.write(b"\0")
            self.file.flush()
        self.file.seek(0)
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.file.close()
            self.file = None
            raise RuntimeError("Another process owns this run; wait for it to stop") from None
        return self

    def __exit__(self, *exc):
        if self.file:
            self.file.seek(0)
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.file, fcntl.LOCK_UN)
            self.file.close()
            self.file = None


def atomic_text(path, content):
    temp = path.with_name(path.name + ".tmp")
    with temp.open("w", encoding="utf-8", newline="") as out:
        out.write(content)
        out.flush()
        os.fsync(out.fileno())
    os.replace(temp, path)


def connect(folder, readonly=False):
    path = (Path(folder) / "state.sqlite3").resolve()
    if not path.is_file():
        raise ValueError(f"No run database: {path}")
    db = sqlite3.connect(path.as_uri() + ("?mode=ro" if readonly else "?mode=rw"), uri=True, timeout=5)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys=ON")
    if not readonly:
        db.execute("PRAGMA synchronous=FULL")
    return db


def meta(db, name):
    row = db.execute("SELECT value FROM metadata WHERE name=?", (name,)).fetchone()
    if row is None:
        raise ValueError(f"Missing run metadata: {name}")
    return json.loads(row[0])


def set_meta(db, name, value):
    db.execute("INSERT OR REPLACE INTO metadata VALUES (?,?)", (name, encode(value)))


def create_run(cfg, handshake, runner_version=""):
    if cfg["algorithm"]["iterations"] < 1:
        raise ValueError("Specify a positive iteration budget in the config or with --iterations")
    cfg = json.loads(encode(cfg))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    folder = Path(cfg["paths"]["runs"]) / run_id
    folder.mkdir(parents=True)
    assets = folder / "assets"
    assets.mkdir()
    files = {}
    original_paths = dict(cfg["paths"])
    try:
        original_code = code_digest()
        # Keep the exact runnable tuner, not just a hash that would require an
        # old checkout to be available after a future upgrade.
        for source in sorted(ROOT.glob("*.py")) + sorted((ROOT / "engine").glob("*")):
            target = folder / "tuner/spsa" / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            with target.open("rb+") as persisted:
                os.fsync(persisted.fileno())
            files[str(target.relative_to(folder))] = digest(target)
            if digest(source) != files[str(target.relative_to(folder))]:
                raise ValueError("Tuner changed during snapshot")
        if original_code != code_digest():
            raise ValueError("Tuner changed during snapshot")
        for key, target in (("engine", assets / "Tunguska.exe"), ("openings", assets / "openings.epd")):
            source = Path(cfg["paths"][key])
            before = digest(source)
            shutil.copyfile(source, target)
            with target.open("rb+") as persisted:
                os.fsync(persisted.fileno())
            if digest(target) != before or digest(source) != before:
                raise ValueError(f"{key} changed while taking its snapshot")
            files[str(target.relative_to(folder))] = before
            cfg["paths"][key] = str(target)
        for dll in Path(original_paths["engine"]).parent.glob("*.dll"):
            target = assets / dll.name
            shutil.copyfile(dll, target)
            with target.open("rb+") as persisted:
                os.fsync(persisted.fileno())
            if digest(dll) != digest(target):
                raise ValueError("Runtime DLL changed during snapshot")
            files[str(target.relative_to(folder))] = digest(target)
        count = 0
        index = assets / "openings.idx"
        with (assets / "openings.epd").open("rb") as book, index.open("wb") as out:
            while True:
                offset = book.tell()
                line = book.readline()
                if not line:
                    break
                if line.strip() and not line.lstrip().startswith(b"#"):
                    out.write(struct.pack("<Q", offset))
                    count += 1
            out.flush()
            os.fsync(out.fileno())
        if not count:
            raise ValueError("Opening suite is empty")
        files[str(index.relative_to(folder))] = digest(index)
        atomic_text(folder / "config.resolved.json", json.dumps(cfg, indent=2) + "\n")
        atomic_text(folder / "uci.txt", "\n".join(handshake) + "\n")
        atomic_text(folder / "cutechess-version.txt", runner_version)
        files["config.resolved.json"] = digest(folder / "config.resolved.json")
        manifest = dict(version=1, run_id=run_id, created=utc(), python=platform.python_version(),
                        code=original_code, original_paths=original_paths, files=files,
                        cutechess_hash=digest(cfg["paths"]["cutechess"]), openings_count=count)
        atomic_text(folder / "manifest.json", json.dumps(manifest, indent=2) + "\n")
        db = sqlite3.connect(folder / "state.sqlite3")
        try:
            db.execute("PRAGMA journal_mode=WAL")
            db.execute("PRAGMA synchronous=FULL")
            db.executescript('''
                CREATE TABLE metadata(name TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE iterations(k INTEGER PRIMARY KEY, step TEXT NOT NULL, theta_before TEXT NOT NULL,
                    theta_after TEXT, rng_after TEXT NOT NULL, created TEXT NOT NULL, committed TEXT);
                CREATE TABLE pairs(k INTEGER NOT NULL REFERENCES iterations(k), pair_id INTEGER NOT NULL,
                    spec TEXT NOT NULL, result TEXT, PRIMARY KEY(k,pair_id));
                CREATE TABLE attempts(id INTEGER PRIMARY KEY, k INTEGER NOT NULL, pair_id INTEGER NOT NULL,
                    folder TEXT NOT NULL UNIQUE, started TEXT NOT NULL, finished TEXT, error TEXT,
                    FOREIGN KEY(k,pair_id) REFERENCES pairs(k,pair_id));
            ''')
            with db:
                set_meta(db, "cfg", cfg)
                set_meta(db, "manifest", manifest)
                set_meta(db, "theta", {n: p["start"] for n, p in cfg["parameters"].items()})
                set_meta(db, "rng", random.Random(cfg["algorithm"]["seed"]).getstate())
                set_meta(db, "iteration", 0)
                set_meta(db, "status", "ready")
        finally:
            db.close()
    except BaseException as exc:
        atomic_text(folder / "initialization-error.txt", str(exc))
        raise
    return folder


def verify(folder, db):
    if db.execute("PRAGMA quick_check").fetchone()[0] != "ok":
        raise ValueError("Run database integrity check failed")
    manifest = meta(db, "manifest")
    if manifest["version"] != 1 or manifest["code"] != code_digest():
        raise ValueError("Tuner or parameter registry changed; restore this run's original version before resuming")
    if manifest["python"] != platform.python_version():
        raise ValueError("Python version changed; resume with the original Python version")
    for name, expected in manifest["files"].items():
        if digest(Path(folder) / name) != expected:
            raise ValueError(f"Run snapshot changed: {name}")
    cfg = meta(db, "cfg")
    if digest(cfg["paths"]["cutechess"]) != manifest["cutechess_hash"]:
        raise ValueError("cutechess executable changed")
    # Snapshots are addressed relative to the supplied run folder, allowing a
    # folder to be moved without falling back to a mutable original engine.
    cfg["paths"]["engine"] = str(Path(folder).resolve() / "assets/Tunguska.exe")
    cfg["paths"]["openings"] = str(Path(folder).resolve() / "assets/openings.epd")
    return cfg


def read_opening(folder, number):
    assets = Path(folder) / "assets"
    with (assets / "openings.idx").open("rb") as index:
        index.seek(number * 8)
        offset = struct.unpack("<Q", index.read(8))[0]
    with (assets / "openings.epd").open("rb") as book:
        book.seek(offset)
        opening = book.readline().decode("utf-8-sig").strip()
    fen_identity(opening)
    return opening


def ensure_iteration(db, folder):
    cfg = meta(db, "cfg")
    k = meta(db, "iteration") + 1
    existing = db.execute("SELECT step FROM iterations WHERE k=?", (k,)).fetchone()
    if existing:
        return json.loads(existing[0])
    if k > cfg["algorithm"]["iterations"]:
        return None
    theta = meta(db, "theta")
    rng = restore_random(meta(db, "rng"))
    step = prepare(cfg, theta, k, rng)
    manifest = meta(db, "manifest")
    count = manifest["openings_count"]
    pairs = []
    for pair_id in range(cfg["match"]["pairs_per_iteration"]):
        opening_id = rng.randrange(count)
        pairs.append(dict(pair_id=pair_id, opening_id=opening_id, opening=read_opening(folder, opening_id),
                          seed=rng.randrange(1, 2**31), event=f"{manifest['run_id']}-k{k}-p{pair_id}"))
    with db:
        db.execute("INSERT INTO iterations VALUES (?,?,?,NULL,?,?,NULL)",
                   (k, encode(step), encode(theta), encode(rng.getstate()), utc()))
        db.executemany("INSERT INTO pairs VALUES (?,?,?,NULL)", [(k, p["pair_id"], encode(p)) for p in pairs])
    return step


def begin_attempt(db, folder, k, pair_id):
    attempt = Path(folder) / "matches" / f"{k:08d}" / f"{pair_id:04d}" / uuid.uuid4().hex[:12]
    attempt.mkdir(parents=True)
    with db:
        cursor = db.execute("INSERT INTO attempts(k,pair_id,folder,started) VALUES (?,?,?,?)",
                            (k, pair_id, str(attempt), utc()))
    return cursor.lastrowid, attempt


def commit_pair(db, k, pair_id, attempt_id, result):
    # Identity check precedes any data write, including idempotent replays.
    attempt = db.execute("SELECT k,pair_id FROM attempts WHERE id=?", (attempt_id,)).fetchone()
    if attempt is None or tuple(attempt) != (k, pair_id):
        raise ValueError("Attempt belongs to a different pair")
    if len(result["games"]) != 2 or result["score"] != sum(g["score"] for g in result["games"]):
        raise ValueError("Invalid completed pair")
    with db:
        row = db.execute("SELECT result FROM pairs WHERE k=? AND pair_id=?", (k, pair_id)).fetchone()
        if row[0] is not None:
            if row[0] != encode(result):
                raise ValueError("Conflicting duplicate pair result")
            return
        db.execute("UPDATE pairs SET result=? WHERE k=? AND pair_id=?", (encode(result), k, pair_id))
        db.execute("UPDATE attempts SET finished=? WHERE id=?", (utc(), attempt_id))


def commit_iteration(db, k, fault=None):
    """Exactly one transaction owns theta, RNG and iteration advancement."""
    with db:
        current = meta(db, "iteration")
        if current >= k:
            return False
        if current + 1 != k:
            raise ValueError("Out-of-order optimizer update")
        row = db.execute("SELECT * FROM iterations WHERE k=?", (k,)).fetchone()
        pairs = db.execute("SELECT result FROM pairs WHERE k=? ORDER BY pair_id", (k,)).fetchall()
        if not pairs or any(r[0] is None for r in pairs):
            raise ValueError("Cannot commit incomplete iteration")
        theta = update(meta(db, "cfg"), json.loads(row["theta_before"]), json.loads(row["step"]),
                       [json.loads(p[0])["score"] for p in pairs])
        set_meta(db, "theta", theta)
        if fault:
            fault("after_theta")
        set_meta(db, "rng", json.loads(row["rng_after"]))
        set_meta(db, "iteration", k)
        db.execute("UPDATE iterations SET theta_after=?,committed=? WHERE k=?", (encode(theta), utc(), k))
    return True


def export_csv(db, folder):
    folder = Path(folder)
    cfg = meta(db, "cfg")
    sequence = meta(db, "iteration")
    def write(name, fields, rows):
        path = folder / name
        temp = path.with_name(path.name + ".tmp")
        with temp.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, ["export_sequence"] + fields, extrasaction="ignore")
            writer.writeheader()
            for row in rows:
                writer.writerow(dict(export_sequence=sequence, **row))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)

    def games():
        for row in db.execute("SELECT k,pair_id,spec,result FROM pairs WHERE result IS NOT NULL ORDER BY k,pair_id"):
            spec, result = json.loads(row["spec"]), json.loads(row["result"])
            for game in result["games"]:
                yield dict(k=row["k"], pair_id=row["pair_id"], opening_id=spec["opening_id"],
                           pair_elapsed=result["elapsed"], artifacts=result["artifacts"], **game)
    write("games.csv", ["k", "pair_id", "opening_id", "game", "white", "black", "result", "score", "reason", "time_forfeit", "pair_elapsed", "artifacts"], games())

    def iterations():
        for it in db.execute("SELECT k,created,committed FROM iterations ORDER BY k"):
            results = [json.loads(r[0]) for r in db.execute("SELECT result FROM pairs WHERE k=? AND result IS NOT NULL", (it["k"],))]
            scores = [g["score"] for r in results for g in r["games"]]
            yield dict(k=it["k"], committed=it["committed"], created=it["created"], pairs=len(results),
                       wins=scores.count(1), draws=scores.count(0), losses=scores.count(-1),
                       elapsed=sum(r["elapsed"] for r in results),
                       **{f"pair_{s+2}": sum(r["score"] == s for r in results) for s in range(-2, 3)})
    write("iterations.csv", ["k", "created", "committed", "pairs", "wins", "draws", "losses", "elapsed"] + [f"pair_{i}" for i in range(5)], iterations())

    def parameters():
        for it in db.execute("SELECT * FROM iterations ORDER BY k"):
            step = json.loads(it["step"])
            after = json.loads(it["theta_after"]) if it["theta_after"] else {}
            for name, d in step["details"].items():
                yield dict(k=it["k"], name=name, start=cfg["parameters"][name]["start"],
                           after=after.get(name, ""), **d)
    write("parameters.csv", ["k", "name", "start", "theta", "c", "a", "delta", "plus", "minus", "clipped", "identical", "after"], parameters())
    theta = meta(db, "theta")
    final = [dict(name=n, selected=n in cfg["selected"], start=p["start"], theta=theta[n],
                  uci=p["uci"], value=nearest(theta[n]), scale=p["scale"], real_value=nearest(theta[n]) / p["scale"])
             for n, p in cfg["parameters"].items()]
    write("final_parameters.csv", ["name", "selected", "start", "theta", "uci", "value", "scale", "real_value"], final)
    atomic_text(folder / "parameters.uci.txt", "\n".join(f"setoption name {r['uci']} value {r['value']}" for r in final) + "\nisready\n")
    atomic_text(folder / "defaults.cpp.txt", "// Candidate defaults in wire units; review before editing parameters.def.\n" +
                "\n".join(f"inline constexpr int {r['name']} = {r['value']};" for r in final) + "\n")
    # Publish marker last. Individual CSVs are replace-atomic; the marker hashes
    # detect a mixed export if power is lost between replacements.
    names = ["games.csv", "iterations.csv", "parameters.csv", "final_parameters.csv", "parameters.uci.txt", "defaults.cpp.txt"]
    atomic_text(folder / "export.json", json.dumps(dict(sequence=sequence, files={n: digest(folder / n) for n in names}), indent=2) + "\n")
