# Wordfish

![Minimal Wordfish emblem](assets/wordfish-logo.svg)

## Introduction

Wordfish is a strong Universal Chess Interface (UCI) engine derived from Stockfish. It preserves the world-class search fundamentals of its predecessor while adding bespoke analysis tools, neural evaluation management, and an experience system tailored for long-term preparation and engine development.

## Engine highlights

- **Dual-network NNUE evaluation** that keeps big and small networks synchronized for efficiency on heterogeneous hardware.
- **Configurable search pipeline** with both classical alpha-beta and hybrid Monte Carlo Tree Search (MCTS) strategies.
- **Persistent experience system** capable of learning from previous searches, storing results in `.exp` files, and serving tailored book moves.
- **NUMA-aware threading** that adapts thread placement dynamically and reports topology changes to the user.
- **Comprehensive diagnostics** including detailed info strings, evaluation tracing, benchmarking, and neural network export facilities.

## Integrated modules and options

Wordfish exposes its capabilities through UCI options. The following overview lists the primary configuration areas and their purposes.

### Core configuration

| Option | Default | Purpose |
| --- | --- | --- |
| `Debug Log File` | *(empty)* | Enables verbose logging to the specified file and starts the logger immediately. |
| `NumaPolicy` | `auto` | Chooses how Wordfish binds threads across NUMA nodes (`auto`, `system`, `hardware`, `none`, or a custom mask). |
| `Threads` | `1` | Sets the number of parallel search threads. Changing the value rebalances the thread pool and shared networks. |
| `Hash` | `16` | Allocates the transposition table size in megabytes. |
| `Clear Hash` | button | Clears cached search data and resets experience buffers for a fresh session. |
| `Ponder` | `false` | Allows the engine to think on the opponent's time. |
| `MultiPV` | `1` | Requests multiple principal variations for analysis sessions. |
| `Search Strategy` | `AlphaBeta` (from `AlphaBeta MCTS Montecarlo`) | Switches between the traditional alpha-beta searcher and the integrated MCTS driver (the `Montecarlo` alias also selects MCTS). |
| `Skill Level` | `20` | Limits playing strength by reducing tactical depth. |
| `Move Overhead` | `10` | Reserves milliseconds per move to avoid time losses. |
| `Minimum Thinking Time` | `100` | Guarantees a minimal allocation of milliseconds per move. |
| `Slow Mover` | `100` | Adjusts how aggressively the engine spends remaining time. |
| `nodestime` | `0` | Sets a node-based time limit for debugging scenarios. |
| `UCI_Chess960` | `false` | Enables Chess960 (Fischer Random) support. |
| `UCI_LimitStrength` | `false` | Activates Elo-limited play in conjunction with `UCI_Elo`. |
| `Contemp` | `0` | Legacy-compatible draw bias that mirrors `Contempt` for existing GUI profiles. |
| `Contempt` | `0` | Biases evaluations toward or against draws. Use `default` to reset to the shipped value. |
| `King Safety` | `100` | Tunes the relative importance of king safety heuristics. Use `default` to reset to the shipped value. |
| `UCI_Elo` | `1320` (range `1320` – `3190`) | Specifies the target Elo when strength limiting is enabled. |
| `UCI_ShowWDL` | `false` | Adds win/draw/loss probabilities to info output. |

### Search strategy and Monte Carlo mode

- The `Search Strategy` option toggles between `AlphaBeta`, `MCTS`, and the alias `Montecarlo`. Selecting either Monte Carlo mode routes move selection through the integrated tree searcher while preserving UCI output such as nodes, NPS, and PVs.
- In Monte Carlo mode the engine reports simulated playouts as nodes, applies the `MultiPV` limit when formatting principal variations, and still honours UCI flags such as `ponder` and `infinite`.
- The `Contemp` and `Contempt` settings can bias the Monte Carlo evaluator towards or away from draws, while `King Safety` scales how aggressively the search protects each monarch across both strategies.

### Neural evaluation management

| Option | Default | Purpose |
| --- | --- | --- |
| `EvalFile` | Embedded big network | Loads or swaps the primary NNUE network file and automatically reloads replicas on all threads. |
| `EvalFileSmall` | Embedded small network | Controls the auxiliary lightweight network for low-latency tasks. |

The `export_net` console command saves both networks, while `trace_eval` prints a complete evaluation breakdown for the current position.

### Experience system

| Option | Default | Purpose |
| --- | --- | --- |
| `Experience Enabled` | `true` | Activates writing and reading from the Wordfish experience file. |
| `Experience File` | `Wordfish.exp` | Sets the storage path for accumulated search knowledge. |
| `Experience Readonly` | `false` | Permits analysis without mutating the experience store. |
| `Experience Book` | `false` | When enabled, consults the learned book during move ordering. |
| `Experience Book Width` | `1` | Limits how many candidate moves are taken from the learned book (1–32). |
| `Experience Book Eval Importance` | `5` | Balances evaluation score versus visit count when ranking book moves. |
| `Experience Book Min Depth` | `27` | Minimum depth a position must reach before being persisted. |
| `Experience Book Max Moves` | `16` | Stops consulting the learned book after this many plies in a single game. |
| `Experience Status` | button | Prints a human-readable summary of the current experience database. |
| `Experience Sync` | button | Forces an immediate flush of accumulated knowledge to disk. |

### Tablebases, books, and specialized tooling

| Option | Default | Purpose |
| --- | --- | --- |
| `SyzygyPath` | *(empty)* | Directory list for probing Syzygy tablebases. Reloads paths instantly when changed. |
| `SyzygyProbeDepth` | `1` | Depth from which endgame tablebase probing begins. |
| `Syzygy50MoveRule` | `true` | Toggles enforcement of the fifty-move rule when using tablebases. |
| `SyzygyProbeLimit` | `7` | Maximum tablebase cardinality to consult during search. |

Additional console commands such as `bench`, `speedtest`, `d`, and `compiler` provide benchmarking, throughput diagnostics, board visualization, and compiler metadata respectively.

## Testing and continuous improvement

Wordfish development relies on sequential probability ratio tests (SPRT) executed on FastChess infrastructure. Each candidate change is measured at two time controls—`10s + 0.1` and `60s + 0.1`—to validate both rapid analysis and longer strategic play. When a FastChess SPRT concludes with a result of `h = 1`, the tested patch is merged into the main branch together with the recorded Elo gain so that the improvement history remains auditable.

## Building from source

The source code resides under `src/` together with a platform-aware `Makefile`.

```bash
cd src
make -j profile-build
```

Run `make help` to inspect targets optimized for specific CPU features or operating systems. Binaries embed the default NNUE networks; external network files placed alongside the executable override the embedded versions.

## Running the engine

1. Launch Wordfish within your preferred UCI-compatible interface.
2. Issue the `uci` command to receive the engine identification and supported options.
3. Adjust options via `setoption name <Option> value <Value>`.
4. Use `position` and `go` commands (or your GUI controls) to start analysis or gameplay.

During analysis, Wordfish emits periodic `info` updates that include depth, score, nodes per second, and—when enabled—WDL statistics. The `isready` and `ucinewgame` commands reset internal state and synchronize the experience system.

## File layout

- `src/` – C++ sources for the engine core, search, evaluation, and utilities.
- `nnue/` – Embedded neural network assets for both evaluation sizes.
- `tests/` – Regression tools for perft validation and instrumentation.
- `scripts/` – Helper utilities used for packaging and benchmarking.
- `assets/wordfish-logo.svg` – Minimal emblem representing the Wordfish identity.
- `Copying.txt` and `LICENSE` – Licensing terms under the GNU General Public License v3.

## Contributing

Bug reports and enhancements are welcome through the repository issue tracker. When submitting engine changes, please include the associated FastChess SPRT results, tested time controls, and observed Elo gain to keep the main branch aligned with validated improvements.

## License

Wordfish is distributed under the GNU General Public License version 3, as provided in `Copying.txt`. Redistributions must include the corresponding source code and preserve this license.

