# Benchmarking Procedure

This document outlines how to benchmark **Revolution** against other engines.

## 1. Build the Engine
```bash
cd src
make -j2 build ARCH=x86-64
```

## 2. Perft Validation
Run the bundled perft suite to confirm move generation correctness:
```bash
scripts/perft.sh
```

## 3. Automated Matches
Use `scripts/match.sh` (requires `cutechess-cli`) to play matches:
```bash
scripts/match.sh /path/to/stockfish 50 40/0.4+0.4
scripts/match.sh /path/to/berserk 50 40/0.4+0.4
scripts/match.sh /path/to/obsidian 50 40/0.4+0.4
```
PGN results are stored as `match.pgn`.

## 4. Rating Comparison
Import the PGN files into your preferred tool (such as [Ordo](https://github.com/michaelkeenan/ordo)) to compute Elo differences between engines.

## 5. Parameter Tuning
- `scripts/fishtest_local.sh` runs a local fishtest worker for large-scale tuning sessions.
- `scripts/spsa.py` performs light-weight SPSA tuning using the engine's `bench` command.
