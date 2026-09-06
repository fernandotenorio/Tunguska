from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from spsa.runner import fen_identity, parse_pair, play_pair
from spsa.process import OwnedProcess

OPENING = "4k3/8/8/8/8/8/P7/4K3 w - - 0 1"


def fixture(results=("1-0", "0-1"), reasons=("White mates", "Black mates")):
    output, pgn = [], []
    for i, (result, reason) in enumerate(zip(results, reasons), 1):
        white, black = ("plus", "minus") if i == 1 else ("minus", "plus")
        output.append(f"Finished game {i} ({white} vs {black}): {result} {{{reason}}}")
        pgn.append(f'[Event "test"]\n[Round "{i}"]\n[White "{white}"]\n[Black "{black}"]\n[Result "{result}"]\n[FEN "{OPENING}"]\n\n1. a4 {{comment}} {result}\n')
    return "\n".join(pgn), "\n".join(output)


class RunnerTests(unittest.TestCase):
    def test_color_scoring_and_out_of_order(self):
        pgn, output = fixture()
        output = "\n".join(reversed(output.splitlines()))
        rows = parse_pair(pgn, output, OPENING, "test")
        self.assertEqual([r["score"] for r in rows], [1, 1])

    def test_draw_and_time_forfeit(self):
        pgn, output = fixture(("1/2-1/2", "0-1"), ("Draw by repetition", "White loses on time"))
        rows = parse_pair(pgn, output, OPENING, "test")
        self.assertEqual([r["score"] for r in rows], [0, 1])
        self.assertTrue(rows[1]["time_forfeit"])

    def test_crash_illegal_stall_not_losses(self):
        for reason in ("Black's connection stalls", "Black makes an illegal move", "Black disconnects"):
            pgn, output = fixture(reasons=(reason, "Black mates"))
            with self.assertRaises(ValueError):
                parse_pair(pgn, output, OPENING, "test")

    def test_truncated_duplicate_mismatched(self):
        pgn, output = fixture()
        corruptions = [(pgn, output.splitlines()[0]), (pgn, output + "\n" + output.splitlines()[0]),
                       (pgn.replace('[White "plus"]', '[White "other"]'), output),
                       (pgn.replace('[Event "test"]', '[Event "other"]'), output),
                       (pgn.replace('P7', '1P6'), output), (pgn.rsplit("0-1", 1)[0], output)]
        for a, b in corruptions:
            with self.subTest(a=a[-60:]):
                with self.assertRaises(ValueError):
                    parse_pair(a, b, OPENING, "test")

    def test_adjudication_requires_opt_in(self):
        pgn, output = fixture(("1/2-1/2", "1/2-1/2"), ("Draw by adjudication: maximal game length",) * 2)
        with self.assertRaises(ValueError):
            parse_pair(pgn, output, OPENING, "test")
        self.assertEqual(len(parse_pair(pgn, output, OPENING, "test", max_moves=20)), 2)

    def test_invalid_fen(self):
        for fen in ("garbage", "8/8/8/8/8/8/8/8 w - -", OPENING.replace("P7", "P8")):
            with self.assertRaises(ValueError):
                fen_identity(fen)

    def test_watchdog_and_cancellation(self):
        with tempfile.TemporaryDirectory() as folder:
            cfg = dict(match=dict(pair_timeout=.2))
            pair = dict(opening=OPENING)
            start = time.monotonic()
            children = []
            def tracked_process(*args, **kwargs):
                child = OwnedProcess(*args, **kwargs)
                children.append(child)
                return child
            with patch("spsa.runner.command", return_value=[sys.executable, "-c", "import time; time.sleep(30)"]), \
                 patch("spsa.runner.OwnedProcess", side_effect=tracked_process):
                with self.assertRaises(TimeoutError):
                    play_pair(cfg, {}, pair, folder, threading.Event())
            self.assertLess(time.monotonic() - start, 5)
            self.assertTrue(children)
            self.assertTrue(all(child.proc.poll() is not None for child in children))
            cancel = threading.Event(); cancel.set()
            with self.assertRaises(InterruptedError):
                play_pair(cfg, {}, pair, folder, cancel)
            # Windows can briefly retain inherited file handles even after the
            # process is signaled dead. Check bounded artifact release separately
            # instead of making TemporaryDirectory demand instantaneous deletion.
            log = Path(folder) / "cutechess.log"
            deadline = time.monotonic() + 2
            while True:
                try:
                    log.unlink()
                    break
                except PermissionError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(.01)
