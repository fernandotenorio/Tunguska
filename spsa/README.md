# Tunguska local SPSA tuner

Python 3.11+ and cutechess-cli 1.4.x drive local self-play. Python uses only its standard library. No server, account, or Python package installation is needed. The supplied Windows build helper requires MSVC x64 tools and an AVX2-capable CPU, matching the engine's existing requirements.

## Start a run

Run these commands from the repository root:

```powershell
python -m spsa.build
python -m spsa validate --config spsa/config.toml --profile search
python -m spsa run --config spsa/config.toml --profile search --iterations 10000
```

The last command is an **example production budget**, not a quick test: with the supplied eight opening pairs per iteration it schedules 160,000 games. Choose a budget appropriate for your machine before starting. The shipped `iterations = 0` intentionally requires an explicit budget.

The config already points to your cutechess installation and UHO book. Its relative paths resolve against the config's directory. The defaults are `10+0.1`, one thread per engine, 64 MB hash per engine, and one concurrent game. The startup timeout is 20 seconds and the whole-pair watchdog is 600 seconds. Increase the watchdog for longer time controls.

`concurrency` is the maximum number of independent pair jobs, each playing one game at a time. A game has two engine processes: budget approximately `2 × concurrency × Hash` for hash tables, plus the engines' other memory. Both colors receive the same time control, thread count, and hash size. Pondering is off. `depth = 0` means no depth cap. `max_moves = 0` disables move-count adjudication. Neither score-based draw nor resignation adjudication is enabled.

Each invocation creates a unique folder under `spsa/runs/` and prints its absolute path. Progress reports each committed pair and optimizer iteration.

## Stop, inspect, and resume

Ctrl+C stops the owned cutechess/engine processes, retains committed opening pairs, and exports the available results. Resume using the same run folder:

```powershell
python -m spsa status --run spsa/runs/<run-id>
python -m spsa resume --run spsa/runs/<run-id>
python -m spsa export --run spsa/runs/<run-id>
```

An engine crash, illegal move, stall, malformed output, or watchdog expiry pauses the run. Inspect its logs, correct the external cause, and explicitly acknowledge retrying that failure:

```powershell
python -m spsa resume --run spsa/runs/<run-id> --retry-failed
```

Normal time forfeits count as losses and are flagged separately in `games.csv`. Failed infrastructure jobs are never silently scored or retried. An interrupted pair is replayed in full, even if its first game finished. Successful pairs in the same iteration are retained. Timed games can differ on replay despite identical settings.

Resume uses the frozen run configuration. Editing your working TOML affects **new runs only**. Changing parameters, bounds, gains, iteration budget, or match conditions requires a new run; use exported final values as its starts. The original budget sets the decay schedule and is not extended in place.

A run pins its Python version, tuner source, engine executable, opening suite, and cutechess executable hash. Engine and opening snapshots are stored inside the run. The tuner source is also saved under `tuner/spsa/`: after upgrading this checkout, you can run the saved version by changing to `<run-folder>/tuner` and invoking `python -m spsa resume --run <absolute-run-folder>`. Use the original Python version and cutechess installation. The run folder can be moved; snapshot paths are resolved from its new location. Historical artifact path strings in CSV refer to their original locations.

## Configure the parameter profiles

`config.toml` contains **all 47 supported parameters**, not just those selected for one run. There are three supplied profiles:

- `search`: initially selects eight aspiration, futility, LMR, and quiescence coefficients.
- `pieces`: selects the five non-king material values.
- `time`: selects ten allocation and instability settings.

Edit a profile's `selected` list to choose any combination from the catalog, or add another named profile. Unselected parameters are sent to both engines at their rounded `start` values throughout the run. Keep the number of simultaneous targets manageable for your game budget.

Each entry documents `default`, `engine_min`, `engine_max`, `scale`, `uci`, `group`, `description`, and `source`. These describe the compiled registry. A stale or incomplete catalog is rejected. The editable tuning fields are:

| Field | Meaning |
|---|---|
| `start` | Initial floating-point optimizer value |
| `min`, `max` | Profile search bounds, within the engine's bounds |
| `c_end` | Perturbation magnitude at the final iteration, in wire units; at least 0.5 |
| `r_end` | Final learning gain relative to the squared perturbation |

All numbers use **integer UCI wire units**. For `LMRBase`, `scale = 100`, so `start = 75` means a coefficient of `0.75`, and `c_end = 3.75` means a perturbation of `0.0375`. Integer-valued heuristic thresholds have scale 1. `r_end = 0.002` and the supplied perturbations are conservative starting settings, not calibrated optimal gains for Tunguska.

Boundary clipping and coordinates that round to the same value are reported in `parameters.csv`. If these dominate a run, examine the ranges and perturbation sizes before starting a revised profile. If an entire comparison rounds to identical engines, the tuner stops with a diagnostic rather than spending games on that comparison.

Piece values affect SEE, capture ordering, delta pruning, and board material bookkeeping. **They do not tune NNUE weights or rescale NNUE evaluations.** The king value remains a fixed search sentinel.

Excluded constants are chess rules, mate/invalid-score sentinels and their safety margins, array dimensions, bitboard/piece encodings, polling and SMP implementation constants, and the large move-ordering category offsets. The common MVV-LVA offset is also fixed; the attacker divisor is tunable. These exclusions are deliberate; arbitrary mutation of every literal would mix tuning with changes to the algorithm and engine invariants. Boolean algorithm switches are not introduced, although the existing numerical check-extension amount can be set to zero.

## Algorithm and result interpretation

For iteration `k = 1..N`, each selected parameter receives an independent Rademacher sign `delta` (±1). All opening pairs in that iteration compare the same plus/minus vectors. The default gain schedule is:

```text
A      = A_ratio * N
c_k    = c_end * (N / k)^gamma
a_k    = r_end * c_end^2 * ((A + N) / (A + k))^alpha
plus   = stochastic_round(clip(theta + c_k * delta))
minus  = stochastic_round(clip(theta - c_k * delta))
signal = (plus_wins - plus_losses) / opening_pairs
theta  = clip(theta + a_k * signal * delta / c_k)
```

The defaults are `alpha = 0.602`, `gamma = 0.101`, `A_ratio = 0.1`. Each game contributes +1/0/-1 from the **plus engine's** perspective, including when plus plays Black. Therefore `signal` lies in [-2, 2]. Dividing by the number of pairs makes batch size a noise/throughput tradeoff instead of a hidden learning-rate multiplier. This is a synchronous, batch-normalized adaptation of the [Fishtest SPSA update convention](https://github.com/official-stockfish/fishtest/blob/master/server/fishtest/spsa_workflow.py).

The optimizer retains fractional values. Candidate rounding is unbiased and seeded, with a shared uniform for the two candidates of a coordinate. The denominator remains the nominal `c_k`, even when candidates hit bounds; it is never the rounded difference. This is a constrained chess self-play update, not an Elo gradient measurement or a guarantee of convergence to a global optimum. The stochastic approximation foundation is described in [Spall's SPSA overview](https://www.jhuapl.edu/SPSA/PDF-SPSA/Spall_An_Overview.PDF).

Openings are sampled uniformly with replacement from the local indexed suite. Both games of a pair use the same opening and swapped colors. Cutechess reads the suite as EPD: its FEN halfmove/fullmove counters are initialized according to cutechess's EPD handling, and both games receive the same resulting state. Completed PGNs and console results must agree on identity, color, opening, result, and accepted termination. Game completion order cannot change an optimizer update. No SPRT or other early outcome stopping is used during tuning.

These matches compare changing perturbations. Their aggregate W/D/L is **not** the final candidate's Elo improvement over Tunguska 2.2. Export the final settings and validate them using your existing independent match workflow before adopting them.

## Run artifacts and durability

| Artifact | Contents |
|---|---|
| `config.resolved.json`, `manifest.json` | Frozen settings, software identities, hashes, original paths |
| `assets/` | Engine/runtime DLL snapshots, opening suite, opening byte-offset index |
| `tuner/spsa/` | Exact Python tuner and engine parameter registry sources |
| `state.sqlite3` and SQLite sidecars | Transactional optimizer, pair schedule, results, attempts, random state |
| `matches/<iteration>/<pair>/<attempt>/` | Exact command, single-opening EPD, PGNs and cutechess log |
| `games.csv` | Accepted game results, termination reasons, time forfeits and artifact locations |
| `iterations.csv` | Progress, W/D/L and five pair-score bins (`pair_0` = two losses through `pair_4` = two wins) |
| `parameters.csv` | Perturbations, gains, applied integers, clipping, and floating-point trajectory |
| `final_parameters.csv` | Latest committed values, including scales and rounded export settings |
| `parameters.uci.txt`, `defaults.cpp.txt` | UCI commands and a reviewable C++ defaults snippet |
| `export.json` | CSV/export checksums and committed iteration sequence |

SQLite uses WAL mode and `synchronous=FULL`. One transaction saves the next experiment before launching it. Each accepted two-game pair is committed separately. Another single transaction updates parameters, advances the random state, and increments the iteration. Retried commits cannot duplicate results or updates.

CSV exports are refreshed at iteration boundaries and orderly stops, and can always be regenerated with `export`. They are replaced atomically file by file; `export.json`, written last, detects a mixed export after interruption. The database is the authority if CSVs lag or are damaged. `elapsed` is summed pair wall time, which includes engine startup and can exceed actual elapsed time when jobs overlap.

Windows processes are started suspended, assigned to a kill-on-close Job Object, then resumed. If the Python coordinator is forcibly terminated, its engines and cutechess descendants are terminated too. An OS-backed lock prevents two coordinators from writing one run. `status` may read a live run; `export` requires its exclusive lock.

Keep SQLite's sidecars with the database. Stop a run before making a filesystem copy of its folder. Crash consistency assumes the storage device honors flush requests; checksums detect damaged snapshots, but software cannot repair a failed physical disk. An initialization failure before a run's metadata is committed is recorded separately and has no games to recover.

## Engine integration and extending the catalog

`engine/parameters.def` is the single engine registry. `engine/parameters.h` maps it to C++17 `inline constexpr` values in a normal build and mutable integers plus UCI metadata only under `TUNGUSKA_SPSA`. The normal Visual Studio project needs no tuning build flags. `python -m spsa.build --mode release` also creates an isolated normal executable.

The tuning UCI interface validates integers and bounds, joins active workers before changing a value, rebuilds dependent tables and material values, recomputes existing board material, and clears search heuristics and the transposition table. NNUE weights are not reloaded. Unchanged options are acknowledged without rebuilding state.

To introduce a new parameter, add its registry entry, replace its engine literal, specify safe bounds/scaling and test its dependent state. Explicitly generate a fresh template and reconcile your profiles:

```powershell
python -c "from spsa.config import write_template; write_template('spsa/config.new.toml')"
```

Do not change compiled `default` metadata merely to start a run elsewhere: change `start`. To adopt exported production values, review and edit the corresponding defaults in `parameters.def`, refresh the catalog metadata, and rebuild the normal engine. Exports never modify the engine automatically.

## Verification

```powershell
python -m unittest discover -s spsa/tests -v
python -m spsa.build --test-engine --output spsa/build/engine-checks
& .\spsa\build\engine-checks\Tunguska.exe
```

The Python suite includes hand-calculated updates, noisy-objective optimization, pairing/parser failures, complete catalog validation, SQLite rollback, abruptly killed writers, replay equivalence, export repair, exclusive locks, and Windows child-tree cleanup. The C++ harness checks material bookkeeping and perft with changed values, promotions, en passant, castling and undo, plus MVV-LVA and LMR rebuilding.

For fixed-depth comparisons, save an untouched baseline before changing the engine, build release and tuning executables with the same compiler flags, then run:

```powershell
python -m spsa.tests.verify_engine --baseline spsa/build/baseline/Tunguska.exe --release spsa/build/release/Tunguska.exe --tuning spsa/build/tuning/Tunguska.exe --depth 12
python -m spsa.tests.verify_run
```

The first checks exact depth/score/node/PV equivalence at default parameters and the UCI boundary cases. Its wall times are measurements, not a statistically powered performance claim. The second uses the real configured executable and opening suite, deliberately kills the coordinator after a committed pair, resumes, and verifies that retained pairs are not replayed. It uses a separate short, depth/move-capped smoke profile under `spsa/build/`; it does not change the production config or run a strength test.
