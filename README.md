# Wordfish

<img src="assets/wordfish-logo.svg" alt="Minimal Wordfish emblem" style="max-width: 240px; height: auto;">

## Overview

Wordfish is a Universal Chess Interface (UCI) engine derived from Stockfish. It retains the parent engine's search strength while adding dual-network neural evaluation, a persistent experience store, and configurable Monte Carlo search. The current release is **Wordfish-4.80-040526**, tuned around the latest Stockfish evaluation networks. The codebase is engineered for reproducible testing, efficient NUMA-aware threading, and transparent diagnostics.

## What's new in 4.80-040526

- Rebranded the engine to **Wordfish-4.80-040526** so executable filenames and GUI analysis headers include the compiled-in architecture suffix, such as `Wordfish-4.80-040526-sse41popcnt`, `Wordfish-4.80-040526-avx2`, `Wordfish-4.80-040526-bmi2`, `Wordfish-4.80-040526-fma3`, or `Wordfish-4.80-040526-avx512`.
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
- **Core options**: `Hash` (MB for the transposition table), `Clear Hash` (button), `Ponder`, `MultiPV`, `Skill Level`, `Move Overhead`, `Minimum Thinking Time`, `Panic Time Buffer`, `Slow Mover`, `nodestime`, `UCI_Chess960`, `UCI_LimitStrength` and `UCI_Elo`, `Contempt`/`Contemp`, and `King Safety`.
- **Short-clock safety**: when the active clock drops under a second, the engine enters a panic regime that boosts `Move Overhead`, clamps thinking time to a fraction of the remaining clock, and caps it by `Panic Time Buffer` (default 200 ms). Raising `Minimum Thinking Time` or `Panic Time Buffer` increases the cushion for sudden disconnections or lag.
- **Monte Carlo tuning**: `Search Strategy` accepts `AlphaBeta`, `MCTS`, or the alias `Montecarlo`. When a clock-based limit is provided (`wtime`, `btime`, or `movetime`), the MCTS driver prioritises that timer over the default simulation cap unless `nodes` is explicitly requested.
- **MCTS configuration**: Enable the MCTS driver with `MCTS Enabled` or by selecting it via `Search Strategy`. Fine-tune behaviour with:
  - `MCTS Rollout Depth`: Maximum plies explored during each rollout (default 12, range 4–128).
  - `MCTS Simulations`: Target number of playouts; set to `0` to run until search limits expire (default 5000, up to 1,000,000).
  - `MCTS Explore`: Exploration constant that balances exploitation of strong moves against broader sampling (default 35, range 1–200).
  - Existing time controls (`wtime`, `btime`, `movetime`) and node limits still apply when provided.
- **Experience controls**: `Experience Enabled`, `Experience File`, `Experience Readonly`, `Experience Book`, `Experience Book Width`, `Experience Book Eval Importance`, `Experience Book Min Depth`, `Experience Book Max Moves`, `Experience Status`, and `Experience Sync`.
- **Neural networks**: `EvalFile` and `EvalFileSmall` accept external NNUE files and reload replicas on all threads.
- **Tablebases**: `SyzygyPath`, `SyzygyProbeDepth`, `Syzygy50MoveRule`, and `SyzygyProbeLimit` configure probing depth, scope, and rule enforcement.

## Settings: online time-control ranges (main page)

Use the following ranges to set the **time control** values in the Settings section for popular online servers. Times are expressed as *base time + increment*.

- **Playchess.com**
  - **Bullet**: 60s + 0s up to 120s + 1s
  - **Blitz**: 180s + 0s up to 16 minutes + 3s
  - **Classical**: 16 minutes + 0s up to 120 minutes + 15s
- **Lichess.org**
  - **Bullet**: 60s + 0s up to 120s + 1s
  - **Blitz**: 180s + 0s up to 16 minutes + 3s
  - **Classical**: 16 minutes + 0s up to 120 minutes + 15s

### Recommended UCI values for these time controls

These baseline values are tuned for online play and are safe defaults across the Bullet, Blitz, and Classical ranges above. Apply them in your GUI's engine options (UCI `setoption`).

- `Hash`: **256** (MB)
- `Move Overhead`: **100** (ms)
- `Contempt`: **0**
- `Contemp`: **0**
- `King Safety`: **100**
- `SyzygyProbeDepth`: **1**
- `Syzygy50MoveRule`: **true**
- `SyzygyProbeLimit`: **7**

## Uso de libros Polyglot (alineado con Pullfish)

Wordfish expone dos ranuras configurables de libros Polyglot, `Book1` y `Book2`, para mezclar repertorios o mantener un respaldo. Cada ranura ofrece los mismos controles:

- **Book1 / Book2**: activa la ranura.
- **Book1 File / Book2 File**: ruta al libro `.bin`. Las rutas absolutas evitan problemas de GUI con espacios o directorios de trabajo.
- **Book1 BestBookMove / Book2 BestBookMove**: restringe el juego al movimiento de mayor peso encontrado en el libro.
- **Book1 Depth / Book2 Depth**: limita la profundidad desde la que se extraen jugadas del libro (plies, por defecto 255).
- **Book1 Width / Book2 Width**: controla la amplitud de selección de jugadas (valores altos exploran más alternativas ponderadas).

Para que una GUI (por ejemplo, Fritz, Arena o CuteChess) use el libro sin problemas:

- Coloca el `.bin` junto al ejecutable de Wordfish o en una carpeta simple sin espacios (por ejemplo, `C:\Engines\Wordfish\`).
- Ajusta `Book1 File` con la ruta absoluta y habilita `Book1`. Deja `Book2` deshabilitado salvo que necesites un segundo libro.
- Si la GUI también tiene su propio libro, prueba ambas combinaciones: libro de la GUI apagado con `Book1` encendido (gestión del motor) y libro de la GUI encendido con `Book1` apagado (gestión de la GUI).
- Revisa el log o consola al iniciar para confirmar que la GUI envía `setoption name Book1 value true` y la ruta correcta de `Book1 File`; así verificas que el motor recibe la configuración.
- Si la GUI tiene problemas con rutas largas, acorta el nombre de la carpeta y evita caracteres especiales antes de volver a apuntar `Book1 File`.

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
