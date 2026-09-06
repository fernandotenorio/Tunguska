"""Strict profile validation and the engine's single-source parameter catalog."""
import hashlib
import json
import math
from pathlib import Path
import re
import tomllib

ROOT = Path(__file__).resolve().parent
PATTERN = re.compile(r'^PARAM\((\w+), (\d+), (\d+), (\d+), (\d+), (\w+), "([^"]+)"\)$')


def catalog():
    entries = {}
    for line in (ROOT / "engine/parameters.def").read_text().splitlines():
        if not line.startswith("PARAM("):
            continue
        match = PATTERN.fullmatch(line)
        if not match:
            raise ValueError(f"Invalid registry entry: {line}")
        name, default, low, high, scale, group, description = match.groups()
        entries[name] = dict(default=int(default), engine_min=int(low), engine_max=int(high),
                             scale=int(scale), group=group, description=description, uci="SPSA_" + name)
    if not entries:
        raise ValueError("Empty parameter registry")
    return entries


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def code_digest():
    h = hashlib.sha256()
    for path in sorted(ROOT.glob("*.py")) + sorted((ROOT / "engine").glob("*")):
        h.update(path.name.encode())
        h.update(path.read_bytes())
    return h.hexdigest()


def keys(data, allowed, where):
    if not isinstance(data, dict):
        raise ValueError(f"{where} must be a TOML table")
    unknown = set(data) - set(allowed)
    if unknown:
        raise ValueError(f"Unknown {where} fields: {sorted(unknown)}")


def number(value, name, low, high, integer=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{name} must be finite")
    if integer and not isinstance(value, int):
        raise ValueError(f"{name} must be an integer")
    if not low <= value <= high:
        raise ValueError(f"{name} must be in [{low}, {high}]")
    return value


def load(path, profile, iterations=None):
    path = Path(path).resolve()
    raw = tomllib.loads(path.read_text(encoding="utf-8"))
    keys(raw, {"version", "paths", "match", "algorithm", "parameters", "profiles"}, "configuration")
    if type(raw.get("version")) is not int or raw["version"] != 1:
        raise ValueError("Unsupported configuration version")
    registry = catalog()
    if not isinstance(raw.get("parameters"), dict):
        raise ValueError("parameters must be a TOML table containing the full catalog")
    if set(raw.get("parameters", {})) != set(registry):
        raise ValueError("Configuration must contain exactly the complete engine parameter catalog")
    params = {}
    for name, metadata in registry.items():
        p = raw["parameters"][name]
        keys(p, set(metadata) | {"start", "min", "max", "c_end", "r_end", "source"}, name)
        for key, expected in metadata.items():
            if p.get(key) != expected:
                raise ValueError(f"Stale catalog {name}.{key}: expected {expected!r}")
        lo = number(p["min"], name + ".min", p["engine_min"], p["engine_max"], True)
        hi = number(p["max"], name + ".max", lo, p["engine_max"], True)
        number(p["start"], name + ".start", lo, hi)
        number(p["c_end"], name + ".c_end", 0.5, max(0.5, hi - lo))
        number(p["r_end"], name + ".r_end", 1e-12, 1)
        params[name] = dict(p)
    profiles = raw.get("profiles", {})
    if not isinstance(profiles, dict):
        raise ValueError("profiles must be a TOML table")
    if profile not in profiles:
        raise ValueError(f"Unknown profile {profile!r}; choose from {list(profiles)}")
    for name, p in profiles.items():
        keys(p, {"selected"}, "profile " + name)
        selected = p.get("selected")
        if not isinstance(selected, list) or not selected or any(not isinstance(x, str) for x in selected):
            raise ValueError("Profiles need a nonempty selected parameter list")
        if len(set(selected)) != len(selected) or set(selected) - set(params):
            raise ValueError(f"Duplicate or unknown parameters in profile {name}")
        if any(params[n]["min"] == params[n]["max"] for n in selected):
            raise ValueError(f"Selected parameters in {name} must have nonzero ranges")
    paths = raw["paths"]
    keys(paths, {"engine", "cutechess", "openings", "runs"}, "paths")
    if set(paths) != {"engine", "cutechess", "openings", "runs"} or any(not isinstance(v, str) or not v for v in paths.values()):
        raise ValueError("paths must specify nonempty engine, cutechess, openings and runs paths")
    paths = {key: str((path.parent / value).resolve()) for key, value in paths.items()}
    for key in ("engine", "cutechess", "openings"):
        if not Path(paths[key]).is_file():
            raise ValueError(f"Missing {key}: {paths[key]}")
    match = raw["match"]
    keys(match, {"tc", "threads", "hash_mb", "concurrency", "pairs_per_iteration", "startup_timeout", "pair_timeout", "max_moves", "depth"}, "match")
    if not isinstance(match["tc"], str) or not re.fullmatch(r"(?:[1-9]\d*/)?(?:\d+(?::[0-5]\d)?(?:\.\d+)?)(?:\+\d+(?:\.\d+)?)?", match["tc"]):
        raise ValueError("Use a finite cutechess time control, e.g. 10+0.1 or 40/15:00")
    base = match["tc"].split("/")[-1].split("+")[0]
    if sum(float(x) for x in base.split(":")) <= 0:
        raise ValueError("Time control must have positive base time")
    for name, lo, hi in [("threads", 1, 512), ("hash_mb", 1, 8192), ("concurrency", 1, 256),
                         ("pairs_per_iteration", 1, 1024), ("max_moves", 0, 1000), ("depth", 0, 63)]:
        number(match[name], name, lo, hi, True)
    for name in ("startup_timeout", "pair_timeout"):
        number(match[name], name, 1, 86400 * 30)
    algorithm = dict(raw["algorithm"])
    keys(algorithm, {"iterations", "alpha", "gamma", "A_ratio", "seed"}, "algorithm")
    if iterations is not None:
        algorithm["iterations"] = iterations
    number(algorithm["iterations"], "iterations", 0, 10**8, True)
    number(algorithm["alpha"], "alpha", 0.01, 1)
    number(algorithm["gamma"], "gamma", 0.001, 0.49)
    if algorithm["alpha"] - algorithm["gamma"] <= 0.5:
        raise ValueError("Require alpha - gamma > 0.5 for decaying-noise SPSA gains")
    number(algorithm["A_ratio"], "A_ratio", 0, 10)
    number(algorithm["seed"], "seed", 0, 2**63 - 1, True)
    return dict(version=1, profile=profile, paths=paths, match=match, algorithm=algorithm,
                parameters=params, selected=profiles[profile]["selected"])


def write_template(path):
    """Explicit catalog regeneration; never invoked implicitly by run/resume."""
    lines = ['# All numeric parameter values use integer UCI wire units.',
             '# iterations=0 requires --iterations N when starting a production run.', 'version = 1', '', '[paths]',
             'engine = "build/tuning/Tunguska.exe"',
             "cutechess = 'D:\\Downloads\\cutechess-1.4.0-win64\\cutechess-1.4.0-win64\\cutechess-cli.exe'",
             "openings = 'D:\\Downloads\\fastchess-windows-x86-64\\fastchess-windows-x86-64\\books\\UHO_Lichess_4852_v1.epd'",
             'runs = "runs"', '', '[match]', 'tc = "10+0.1"', 'threads = 1', 'hash_mb = 64',
             'concurrency = 1', 'pairs_per_iteration = 8', 'startup_timeout = 20',
             'pair_timeout = 600', 'max_moves = 0', 'depth = 0', '', '[algorithm]',
             'iterations = 0', 'alpha = 0.602', 'gamma = 0.101', 'A_ratio = 0.1', 'seed = 20260906']
    registry = catalog()
    selection = ["AspirationDelta", "RFPMargin", "FutilityMargin1", "FutilityMargin2", "FutilityMargin3", "LMRBase", "LMRDivisor", "QDeltaMargin"]
    for group in ("search", "pieces", "time"):
        selected = selection if group == "search" else [n for n, p in registry.items() if p["group"] == group]
        lines += ["", f"[profiles.{group}]", "selected = " + json.dumps(selected)]
    for name, p in registry.items():
        source = "Tunguska/src/Engine/Search.cpp"
        if p["group"] == "pieces": source = "Tunguska/includes/Engine/Evaluation.h"
        if p["group"] == "time": source = "Tunguska/includes/Engine/TimeManager.h"
        if name == "TimeInstabilityDepth": source = "Tunguska/src/Engine/Search.cpp"
        if name == "HistoryMax": source = "Tunguska/includes/Engine/Search.h"
        lines += ["", f"[parameters.{name}]"]
        values = dict(p, source=source, start=p["default"], min=p["engine_min"], max=p["engine_max"],
                      c_end=max(0.5, round(p["default"] * 0.05, 2)), r_end=0.002)
        for key, value in values.items():
            lines.append(f"{key} = {json.dumps(value)}")
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")
