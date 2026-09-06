"""Build isolated x64 MSVC executables without modifying VS user settings."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil

ROOT = Path(__file__).resolve().parents[1]


def build(mode="tuning", output=None, test_engine=False):
    dest = Path(output).resolve() if output else ROOT / "spsa" / "build" / mode
    dest.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    if not installation:
        raise RuntimeError("MSVC x64 tools are required")
    vcvars = Path(installation) / "VC/Auxiliary/Build/vcvars64.bat"
    # Only the discovered toolchain path enters this shell command, never profile data.
    output_env = subprocess.check_output(f'cmd.exe /d /s /c ""{vcvars}" >nul && set"', text=True)
    env = os.environ.copy()
    for line in output_env.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key] = value
    main = ROOT / "spsa/tests/engine_checks.cpp" if test_engine else ROOT / "Tunguska/src/Main.cpp"
    sources = sorted((ROOT / "Tunguska/src/Engine").glob("*.cpp")) + sorted((ROOT / "Tunguska/src/NNUE").glob("*.cpp")) + [main]
    exe = dest / "Tunguska.exe"
    compiler = shutil.which("cl.exe", path=next(v for k, v in env.items() if k.upper() == "PATH"))
    if not compiler:
        compiler = str(sorted((Path(installation) / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))[-1])
    args = [compiler, "/nologo", "/std:c++17", "/EHsc", "/O2", "/GL", "/MT", "/arch:AVX2", "/DNDEBUG", "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(ROOT / "Tunguska/includes"), "/Fe:" + str(exe)]
    if mode == "tuning":
        args.append("/DTUNGUSKA_SPSA")
    args += [str(p) for p in sources] + ["/link", "/LTCG", "/OPT:REF", "/OPT:ICF"]
    (dest / "build-command.json").write_text(json.dumps(args, indent=2))
    with (dest / "build.log").open("w") as log:
        result = subprocess.run(args, cwd=dest, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Build failed: {dest / 'build.log'}")
    print(exe)
    return exe


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["tuning", "release"], default="tuning")
    parser.add_argument("--output")
    parser.add_argument("--test-engine", action="store_true")
    args = parser.parse_args()
    build(args.mode, args.output, args.test_engine)
