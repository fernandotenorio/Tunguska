import subprocess
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
import sys

PYTHON_EXEC = sys.executable # virtual env
RAW_DIR = Path("../data/raw")
QUIET_DIR = Path("../data/quiet")
QPE_SCRIPT = "qpe.py"
ENGINE_PATH = r"D:\Downloads\fastchess-windows-x86-64\fastchess-windows-x86-64\engines\stockfish_20011801_x64.exe"
MAX_WORKERS = 10


def run_file(fen_file: Path):
    output_file = QUIET_DIR / (fen_file.stem + ".epd")
    evaluated_file = QUIET_DIR / (fen_file.stem + ".evaluated")

    cmd = [
        PYTHON_EXEC,
        QPE_SCRIPT,
        "--input", str(fen_file),
        "--outputepd", str(output_file),
        "--engine", ENGINE_PATH,
        "--engine-option", "Threads=1",
        "--movetimems", "700",
        "--pvlen", "4",
        "--static-eval",
        "--evaluated-file", str(evaluated_file)
    ]

    print(f"Starting {fen_file.name}")
    result = subprocess.run(cmd)

    if result.returncode != 0:
        print(f"FAILED: {fen_file.name}")

    return fen_file.name


if __name__ == "__main__":
    QUIET_DIR.mkdir(parents=True, exist_ok=True)

    fen_files = list(RAW_DIR.glob("*.epd"))

    with ProcessPoolExecutor(max_workers=MAX_WORKERS) as executor:
        for finished in executor.map(run_file, fen_files):
            print(f"Finished: {finished}")
