"""UCI preflight and strict two-game cutechess jobs."""
import json
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

from .optimizer import nearest
from .process import OwnedProcess

OPTION = re.compile(r"^option name (.+) type spin default (-?\d+) min (-?\d+) max (-?\d+)$")
FINISHED = re.compile(r"^Finished game (\d+) \(([^\n]+) vs ([^\n]+)\): (1-0|0-1|1/2-1/2) \{([^\n]*)\}\s*$", re.M)
TAG = re.compile(r'^\[(\w+) "((?:[^"\\]|\\.)*)"\]$', re.M)


def probe_cutechess(cfg):
    timeout = cfg["match"]["startup_timeout"]
    with OwnedProcess([cfg["paths"]["cutechess"], "-version"], stdin=subprocess.DEVNULL,
                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                      encoding="utf-8", errors="replace") as proc:
        try:
            output, _ = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            raise RuntimeError("cutechess startup/version watchdog expired") from None
        if proc.returncode or not re.search(r"cutechess-cli 1\.4\.\d+", output):
            raise ValueError("This adapter requires cutechess-cli 1.4.x; verify a new version before using it")
        return output


def probe(cfg):
    """Verify all options and explicit engine acknowledgements, with a deadline."""
    timeout = cfg["match"]["startup_timeout"]
    with OwnedProcess([cfg["paths"]["engine"]], cwd=Path(cfg["paths"]["engine"]).parent,
                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                      text=True, encoding="utf-8", errors="replace", bufsize=1) as proc:
        lines = queue.Queue()
        def read():
            for line in proc.stdout:
                lines.put(line.rstrip())
            lines.put(None)
        thread = threading.Thread(target=read, daemon=True)
        thread.start()
        def send(value):
            proc.stdin.write(value + "\n")
            proc.stdin.flush()
        def until(marker):
            deadline = time.monotonic() + timeout
            output = []
            while True:
                try:
                    line = lines.get(timeout=max(0.001, deadline - time.monotonic()))
                except queue.Empty:
                    raise RuntimeError(f"UCI startup timeout waiting for {marker}") from None
                if line is None:
                    raise RuntimeError(f"Engine exited before {marker}: {output[-5:]}")
                output.append(line)
                if line == marker:
                    return output
                if time.monotonic() > deadline:
                    raise RuntimeError(f"UCI startup timeout waiting for {marker}")
        send("uci")
        handshake = until("uciok")
        options = {}
        for line in handshake:
            if m := OPTION.fullmatch(line):
                options[m[1]] = tuple(map(int, m.groups()[1:]))
        expected_names = {p["uci"] for p in cfg["parameters"].values()}
        if {n for n in options if n.startswith("SPSA_")} != expected_names:
            raise ValueError("Tuning executable does not expose the complete expected registry")
        for name, p in cfg["parameters"].items():
            if options.get(p["uci"]) != (p["default"], p["engine_min"], p["engine_max"]):
                raise ValueError(f"UCI catalog mismatch: {name}")
        for name, value in [("Hash", cfg["match"]["hash_mb"]), ("Threads", cfg["match"]["threads"])]:
            if name not in options or not options[name][1] <= value <= options[name][2]:
                raise ValueError(f"Engine does not support requested {name}")
            send(f"setoption name {name} value {value}")
        for p in cfg["parameters"].values():
            send(f"setoption name {p['uci']} value {nearest(p['start'])}")
        send("isready")
        applied = set(until("readyok"))
        for p in cfg["parameters"].values():
            if f"info string spsa applied {p['uci']} {nearest(p['start'])}" not in applied:
                raise ValueError(f"Engine did not acknowledge {p['uci']}")
        send("quit")
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            raise RuntimeError("Engine did not exit after UCI preflight") from None
        thread.join(timeout=1)
        if proc.returncode:
            raise RuntimeError("Engine failed during UCI preflight")
        return handshake


def fen_identity(fen):
    fields = fen.split()
    if len(fields) < 4:
        raise ValueError("Opening lacks the four required FEN fields")
    ranks = fields[0].split("/")
    if len(ranks) != 8 or fields[1] not in ("w", "b"):
        raise ValueError("Invalid opening FEN")
    if not re.fullmatch(r"-|K?Q?k?q?", fields[2]) or not re.fullmatch(r"-|[a-h][36]", fields[3]):
        raise ValueError("Invalid castling/en-passant FEN")
    for rank in ranks:
        if any(c not in "12345678pPnNbBrRqQkK" for c in rank) or sum(int(c) if c.isdigit() else 1 for c in rank) != 8:
            raise ValueError("Invalid FEN rank")
    if fields[0].count("K") != 1 or fields[0].count("k") != 1:
        raise ValueError("Opening must contain both kings")
    return " ".join(fields[:4])


def parse_pair(pgn, output, opening, event, max_moves=0):
    records = {}
    for m in FINISHED.finditer(output):
        game, white, black, result, reason = m.groups()
        game = int(game)
        if game in records or game not in (1, 2):
            raise ValueError("Duplicate or unexpected finished-game identity")
        records[game] = dict(game=game, white=white, black=black, result=result, reason=reason)
    if set(records) != {1, 2}:
        raise ValueError("Expected exactly two completed games in cutechess output")
    blocks = re.split(r'(?=^\[Event ")', pgn, flags=re.M)
    games = [b for b in blocks if b.strip()]
    if len(games) != 2:
        raise ValueError("Expected exactly two complete PGN games")
    observed = set()
    for block in games:
        pairs = TAG.findall(block)
        tags = dict(pairs)
        if len(tags) != len(pairs):
            raise ValueError("Duplicate PGN tags")
        try:
            game = int(tags["Round"])
            row = records[game]
        except (KeyError, ValueError):
            raise ValueError("Unrecognized PGN game identity") from None
        if game in observed:
            raise ValueError("Duplicate PGN game")
        observed.add(game)
        expected = ("plus", "minus") if game == 1 else ("minus", "plus")
        if (row["white"], row["black"]) != expected or (tags.get("White"), tags.get("Black")) != expected:
            raise ValueError("Incorrect engine identity or color pairing")
        if tags.get("Event") != event or tags.get("Result") != row["result"]:
            raise ValueError("PGN and match output disagree")
        if fen_identity(tags.get("FEN", "")) != fen_identity(opening):
            raise ValueError("Pair opening mismatch")
        movetext = TAG.sub("", block)
        movetext = re.sub(r"\{[^}]*\}", "", movetext, flags=re.S).strip()
        if not movetext.endswith(row["result"]):
            raise ValueError("Incomplete PGN movetext")
        termination = tags.get("Termination", "normal").lower()
        reason = row["reason"].lower()
        if any(token in reason for token in ("illegal", "disconnect", "stall", "crash", "error", "connection")):
            raise ValueError(f"Infrastructure/protocol failure: {row['reason']}")
        # Time forfeits are data. Infrastructure/protocol failures are not.
        time_forfeit = "time" in reason and ("forfeit" in reason or "loses" in reason)
        normal_reason = any(x in reason for x in ("mates", "stalemate", "repetition", "fifty", "50 move", "insufficient material", "insufficient mating material"))
        capped = max_moves > 0 and "adjudication" in reason and row["result"] == "1/2-1/2"
        if not (normal_reason or time_forfeit or capped):
            raise ValueError(f"Unaccepted game termination: {row['reason']}")
        if termination not in ("normal", "time forfeit", "adjudication"):
            raise ValueError(f"Abnormal PGN termination: {termination}")
        if time_forfeit and row["result"] == "1/2-1/2":
            raise ValueError("Unexpected drawn time forfeit")
        white_score = {"1-0": 1, "0-1": -1, "1/2-1/2": 0}[row["result"]]
        row["score"] = white_score if row["white"] == "plus" else -white_score
        row["time_forfeit"] = time_forfeit
    return [records[1], records[2]]


def command(cfg, step, pair, attempt):
    m = cfg["match"]
    args = [cfg["paths"]["cutechess"]]
    for side in ("plus", "minus"):
        args += ["-engine", f"name={side}", "cmd=" + cfg["paths"]["engine"],
                 "dir=" + str(Path(cfg["paths"]["engine"]).parent)]
        args += [f"option.{cfg['parameters'][name]['uci']}={value}" for name, value in step[side].items()]
    args += ["-each", "proto=uci", "restart=on", "tc=" + m["tc"],
             f"option.Hash={m['hash_mb']}", f"option.Threads={m['threads']}"]
    if m["depth"]:
        args += [f"depth={m['depth']}"]
    args += ["-rounds", "2", "-games", "1", "-repeat", "-concurrency", "1", "-srand", str(pair["seed"]),
             "-openings", "file=" + str(attempt / "opening.epd"), "format=epd", "order=sequential",
             "-event", pair["event"], "-pgnout", str(attempt / "games.pgn"), "fi"]
    if m["max_moves"]:
        args += ["-maxmoves", str(m["max_moves"])]
    return args


def play_pair(cfg, step, pair, attempt, cancel):
    if cancel.is_set():
        raise InterruptedError("Pair canceled before launch")
    attempt = Path(attempt)
    (attempt / "opening.epd").write_text(pair["opening"] + "\n", encoding="utf-8")
    args = command(cfg, step, pair, attempt)
    (attempt / "command.json").write_text(json.dumps(args, indent=2), encoding="utf-8")
    start = time.monotonic()
    with (attempt / "cutechess.log").open("wb") as log:
        with OwnedProcess(args, cwd=attempt, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT) as proc:
            while proc.poll() is None:
                if cancel.wait(0.1):
                    raise InterruptedError("Pair interrupted; replay on resume")
                if time.monotonic() - start > cfg["match"]["pair_timeout"]:
                    raise TimeoutError("Pair watchdog expired; results not accepted")
            if proc.returncode:
                raise RuntimeError(f"cutechess exited {proc.returncode}; inspect {attempt}")
    rows = parse_pair((attempt / "games.pgn").read_text(encoding="utf-8-sig"),
                      (attempt / "cutechess.log").read_text(encoding="utf-8-sig", errors="replace"),
                      pair["opening"], pair["event"], cfg["match"]["max_moves"])
    return dict(games=rows, score=sum(r["score"] for r in rows), elapsed=time.monotonic() - start,
                artifacts=str(attempt))
