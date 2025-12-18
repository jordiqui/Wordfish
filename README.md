# Wordfish

<img src="assets/wordfish-logo.svg" alt="Minimal Wordfish emblem" style="max-width: 240px; height: auto;">

## Overview

Wordfish is a Universal Chess Interface (UCI) engine derived from Stockfish. It retains the parent engine's search strength while adding dual-network neural evaluation, a persistent experience store, and configurable Monte Carlo search. The current release is **Wordfish-3.60-181225**, tuned around the latest Stockfish evaluation networks. The codebase is engineered for reproducible testing, efficient NUMA-aware threading, and transparent diagnostics.

## What's new in 3.60-181225

- Rebranded the engine to **Wordfish-3.60-181225** and propagated architecture-aware suffixes to both the executable name and UCI identification so GUIs like Fritz 20 and CuteChess show `Wordfish-3.60-181225-sse41popcnt` (SSE4.1/POPCNT builds), `Wordfish-3.60-181225-avx2` (AVX2 builds), or the base `Wordfish-3.60-181225` name for generic targets.
- Updated the main NNUE evaluator to `nn-2962dca31855.nnue` from Stockfish dev 20251130, keeping the paired small network in sync for dual-network evaluation.
- Emphasized king safety, rook coordination, and supervised endgame patterns in recent network training and handcrafted heuristics.
- Tightened king-safety heuristics around open files, rook lifts, and dark-square weaknesses while rewarding coordinated rooks and discouraging premature flank pawn storms.
- Reduced aggressive pruning in sharp positions and introduced a verification search to confirm large swings in evaluation, improving stability in complicated lines.

## Architecture and integrated modules

- **Search pipeline**: Classical alpha–beta search remains the default, with an alternative Monte Carlo Tree Search (MCTS) driver selectable through `Search Strategy`. Both strategies honour standard UCI limits (`depth`, `nodes`, `movetime`, `infinite`, and pondering) and share the same move generator and time manager.
- **Neural evaluation**: A paired NNUE design keeps large and small networks in step. Networks are hot-swapped via `EvalFile` and `EvalFileSmall`, with `export_net` producing portable binaries. The `trace_eval` command prints a complete evaluation trace for the current position.
- **Experience system**: Search outcomes are written to an `.exp` file when `Experience Enabled` is true. `Experience Readonly` permits analysis without mutating the store, while `Experience Sync` forces an immediate flush. Learned moves can be consulted as a lightweight book when `Experience Book` is active.
- **NUMA and threading**: `Threads` resizes the worker pool, and `NumaPolicy` (`auto`, `system`, `hardware`, `none`, or a custom mask) governs thread placement. Changes are reflected immediately in shared network replicas and hash tables.
- **Diagnostics and tooling**: Console commands include `bench`, `speedtest`, `d` (board dump), `compiler` (build metadata), and `export_net`. Info output can include win/draw/loss probabilities through `UCI_ShowWDL`.
- **Endgame knowledge**: Syzygy tablebases are configured with `SyzygyPath`, `SyzygyProbeDepth`, `SyzygyProbeLimit`, and `Syzygy50MoveRule`.

## UCI surface and principal options

- **Session control**: `uci` announces identity and options; `isready` synchronises threads; `ucinewgame` clears experience buffers; `stop` halts search; `ponderhit` resumes after pondering; `quit` exits cleanly.
- **Core options**: `Hash` (MB for the transposition table), `Clear Hash` (button), `Ponder`, `MultiPV`, `Skill Level`, `Move Overhead`, `Minimum Thinking Time`, `Slow Mover`, `nodestime`, `UCI_Chess960`, `UCI_LimitStrength` and `UCI_Elo`, `Contempt`/`Contemp`, and `King Safety`.
- **Monte Carlo tuning**: `Search Strategy` accepts `AlphaBeta`, `MCTS`, or the alias `Montecarlo`. When a clock-based limit is provided (`wtime`, `btime`, or `movetime`), the MCTS driver prioritises that timer over the default simulation cap unless `nodes` is explicitly requested.
- **MCTS configuration**: Enable the MCTS driver with `MCTS Enabled` or by selecting it via `Search Strategy`. Fine-tune behaviour with:
  - `MCTS Rollout Depth`: Maximum plies explored during each rollout (default 12, range 4–128).
  - `MCTS Simulations`: Target number of playouts; set to `0` to run until search limits expire (default 5000, up to 1,000,000).
  - `MCTS Explore`: Exploration constant that balances exploitation of strong moves against broader sampling (default 35, range 1–200).
  - Existing time controls (`wtime`, `btime`, `movetime`) and node limits still apply when provided.
- **Experience controls**: `Experience Enabled`, `Experience File`, `Experience Readonly`, `Experience Book`, `Experience Book Width`, `Experience Book Eval Importance`, `Experience Book Min Depth`, `Experience Book Max Moves`, `Experience Status`, and `Experience Sync`.
- **Neural networks**: `EvalFile` and `EvalFileSmall` accept external NNUE files and reload replicas on all threads.
- **Tablebases**: `SyzygyPath`, `SyzygyProbeDepth`, `Syzygy50MoveRule`, and `SyzygyProbeLimit` configure probing depth, scope, and rule enforcement.

## Building

The sources live in `src/`. Build with optimised profiling settings by running:

```bash
cd src
make -j profile-build
```

Use `make help` for platform-specific targets. Bundled binaries embed default NNUE networks; co-located external networks override the embedded copies at runtime.

## Running the engine

1. Start your preferred UCI-compatible interface and load the Wordfish binary.
2. Issue `uci` to confirm available options, then adjust settings with `setoption name <Option> value <Value>`.
3. Provide a position via `position` (FEN or move list) and start calculation with `go` plus the desired limits.
4. During analysis, monitor `info` strings for depth, score, nodes per second, and—when enabled—win/draw/loss figures.

## Suggested MCTS evaluation profile

For Monte Carlo analysis sessions, start from the following balanced configuration and adjust to taste:

- `Search Strategy`: `MCTS`
- `MCTS Rollout Depth`: `20` (keeps playouts tactical without drifting too far from the frontier)
- `MCTS Simulations`: `8000` (set to `0` to let the clock dictate stopping conditions)
- `MCTS Explore`: `40` (slightly more exploratory than the default for broader coverage)
- `Move Overhead`: `30` (ms buffer on fixed `movetime` runs to avoid time forfeits)

Example commands:

```bash
setoption name Search Strategy value MCTS
setoption name MCTS Rollout Depth value 20
setoption name MCTS Simulations value 8000
setoption name MCTS Explore value 40
setoption name Move Overhead value 30
```

## Validation and release discipline

Candidate changes are measured with Sequential Probability Ratio Tests (SPRT) on FastChess at short (`10s + 0.1`) and longer (`60s + 0.1`) controls. Patches that return `h = 1` are merged with their recorded Elo gains so that the performance history remains auditable.

## File layout

- `src/` – C++ implementation of search, evaluation, and utilities.
- `nnue/` – Embedded neural networks for both evaluation sizes.
- `tests/` – Regression and perft tooling.
- `scripts/` – Packaging and benchmarking helpers.
- `assets/wordfish-logo.svg` – Minimal Wordfish emblem.
- `Copying.txt` and `LICENSE` – GNU General Public License v3 terms.

## Contribution guidelines

Please accompany functional changes with FastChess SPRT evidence, including tested time controls and observed Elo impact. Bug reports and enhancement proposals are welcomed through the issue tracker, with reproducible steps and expected outcomes.

## Licence

Wordfish is distributed under the GNU General Public License version 3. Redistributions must include corresponding source code and preserve this licence text.
