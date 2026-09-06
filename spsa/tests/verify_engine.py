"""Fixed-depth regression and bounded UCI integration checks against a saved baseline."""
import argparse
import json
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

from spsa.config import catalog
from spsa.process import OwnedProcess

POSITIONS = ["startpos", "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
             "fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "fen 4k3/P7/8/8/8/8/7p/4K3 w - - 0 1",
             "fen rnb1kbnr/ppp1pppp/8/4q3/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4"]


class Session:
    def __init__(self, executable):
        self.owned = OwnedProcess([str(Path(executable).resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.p = self.owned.proc
        self.q = queue.Queue()
        def reader():
            for line in self.p.stdout: self.q.put(line.strip())
            self.q.put(None)
        self.thread = threading.Thread(target=reader, daemon=True)
        self.thread.start()

    def send(self, line):
        self.p.stdin.write(line + "\n"); self.p.stdin.flush()

    def until(self, prefix, timeout=60):
        deadline = time.monotonic() + timeout
        lines = []
        while True:
            line = self.q.get(timeout=max(.001, deadline - time.monotonic()))
            if line is None: raise RuntimeError("Engine exited: " + str(lines))
            lines.append(line)
            if line.startswith(prefix): return lines
            if time.monotonic() >= deadline: raise TimeoutError(prefix)

    def close(self):
        self.send("quit")
        self.p.wait(timeout=10)
        self.thread.join(timeout=1)
        self.owned.close()


def measure(path, depth):
    session = Session(path)
    try:
        session.send("uci"); handshake = session.until("uciok")
        session.send("setoption name Hash value 64")
        session.send("setoption name Threads value 1")
        session.send("isready"); session.until("readyok")
        records = []
        start = time.perf_counter()
        for fen in POSITIONS:
            session.send("ucinewgame")
            session.send("position " + fen)
            session.send(f"go depth {depth}")
            lines = session.until("bestmove")
            records.append([re.sub(r" time \d+", "", line) for line in lines if line.startswith(("info depth", "bestmove"))])
        return records, time.perf_counter() - start, handshake
    finally:
        session.close()


def tuning_checks(path):
    session = Session(path)
    try:
        session.send("uci"); session.until("uciok")
        session.send("setoption name Hash value 16")
        for name, p in catalog().items():
            for value in (p["engine_min"], p["engine_max"], p["default"]):
                session.send(f"setoption name {p['uci']} value {value}")
                session.send("isready")
                lines = session.until("readyok")
                assert f"info string spsa applied {p['uci']} {value}" in lines, lines
            for value in (p["engine_min"] - 1, p["engine_max"] + 1, "1junk", "1 extra"):
                session.send(f"setoption name {p['uci']} value {value}")
                session.send("isready")
                assert f"info string spsa rejected {p['uci']}" in session.until("readyok")
        session.send("position startpos")
        session.send("setoption name Threads value 2")
        session.send("go infinite")
        session.until("info depth 4")
        session.send("setoption name SPSA_PawnValue value 150")
        session.send("isready")
        assert "info string spsa applied SPSA_PawnValue 150" in session.until("readyok")
        session.send("position startpos moves e2e4")
        session.send("go depth 5")
        session.until("bestmove")
        session.send("setoption name Threads value 1")
        for bound in ("engine_min", "engine_max"):
            for p in catalog().values():
                session.send(f"setoption name {p['uci']} value {p[bound]}")
            session.send("isready"); session.until("readyok")
            for fen in POSITIONS:
                session.send("ucinewgame")
                session.send("position " + fen)
                session.send("go depth 6")
                lines = session.until("bestmove")
                assert lines[-1] != "bestmove 0000", lines
    finally:
        session.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--tuning", required=True)
    parser.add_argument("--depth", type=int, default=8)
    args = parser.parse_args()
    measurements = {name: measure(getattr(args, name), args.depth) for name in ("baseline", "release", "tuning")}
    assert measurements["baseline"][0] == measurements["release"][0], "Normal build search regression"
    assert measurements["baseline"][0] == measurements["tuning"][0], "Tuning defaults search regression"
    assert not any("SPSA_" in line for line in measurements["release"][2]), "Tuning options leaked into release"
    tuning_checks(args.tuning)
    print(json.dumps({"fixed_depth_equivalence": "passed", "uci_validation": "passed",
                      "suite_wall_seconds": {n: m[1] for n, m in measurements.items()}}, indent=2))
