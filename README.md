# Wordfish 2.0 dev Chess Engine (010925)

<div align="center">
  <h3>Wordfish 2.0 dev</h3>

  A free and open-source UCI chess engine combining classical algorithms with neural network innovations.
  <br>
  <em>Authors: Stockfish developers, Jorge Ruiz, ChatGPT</em>

</div>

## Overview

**Wordfish** is a free, open-source UCI chess engine implementing cutting-edge search algorithms combined with neural network evaluation. Derived from fundamental chess programming principles, Wordfish analyzes positions through parallelized alpha-beta search enhanced with null-move pruning and late move reductions.

As a UCI-compliant engine, Wordfish operates through **standard chess interfaces** without an integrated graphical interface. Users must employ compatible chess GUIs (Arena, Scid vs PC, etc.) for board visualization and move input. Consult your GUI documentation for implementation details.

## Technical Architecture

Wordfish's architecture features:

- Hybrid evaluation system combining classical heuristics with NNUE networks
- SMP parallelization with YBWC (Young Brothers Wait Concept)
- Advanced pruning techniques (Reverse Futility Pruning, Late Move Pruning)
- Efficient move ordering with history heuristics and killer moves
- Tuned search parameters through reinforcement learning

## Files

The distribution includes:

- `README.md` (this documentation)
- `COPYING.txt` ([GNU GPLv3 license][gpl-link])
- `AUTHORS` (contributor acknowledgments)
- `src/` (source code with platform-specific Makefiles)
 - Neural network weights (`*.nnue`)

## Contributing

### Development Guidelines
Contributions must adhere to:
- Clean, documented C++17 implementations
- Benchmark validation through perft testing
- Elo measurement via [OpenBench][openbench-link]
- Compatibility with UCI protocol standard

### Testing Infrastructure
 Improvements require extensive testing:
 - Install the [Wordfish Test Worker][worker-link]
 - Participate in active tests on [Wordfish Test Suite][testsuite-link]
 - Verify ELO gains through SPRT validation

### Community
 Technical discussions occur primarily through:
 - [Wordfish Discord Server][discord-link]
 - [GitHub Discussions][discussions-link]
 - [Chess Programming Wiki][chesswiki-link]

## Compilation

Compile from source using included Makefiles:
```bash
cd src
make -j ARCH=x86-64-modern
```

Supported architectures:
- `x86-64`: Modern x86 processors
- `armv8`: ARMv8+ architectures
- `ppc64`: PowerPC systems

Full compilation guides available in [documentation][doc-link].

## Syzygy Tablebases

 Wordfish can probe [Syzygy](https://github.com/syzygy1) endgame tablebases when a
 directory is supplied via the `SyzygyPath` UCI option. The engine also exposes a
 `SyzygyPremap` boolean option. When set to `true`, `Tablebases::init` pre-maps all
 available WDL and DTZ tables during initialization, reducing probe latency at the
 expense of additional startup time and memory usage.

## Slow Mover Option

 The `Slow Mover` UCI option controls how aggressively the engine spends its
 available time during a game. Higher values make Wordfish use a larger
 percentage of its clock on each move, playing more slowly, while lower values
 encourage faster play. This setting only affects game play and has no impact
 when running analysis.

## Experience Book

 Wordfish puede aprender de partidas previas guardando datos en un archivo `.exp` en formato binario.
Las siguientes opciones UCI controlan este sistema:

- `Experience Enabled`: activa o desactiva la experiencia (por defecto `true`).
 - `Experience File`: nombre del archivo donde se almacena la experiencia (por defecto `wordfish.exp`).
- `Experience Readonly`: si es `true`, no se escriben cambios en el archivo.
- `Experience Book`: usa la experiencia como libro de aperturas.
- `Experience Book Width`: número de movimientos principales a considerar (1–20).
- `Experience Book Eval Importance`: ponderación de la evaluación al ordenar movimientos (0–10).
- `Experience Book Min Depth`: profundidad mínima para almacenar un movimiento (4–64).
- `Experience Book Max Moves`: máximo de movimientos guardados por posición (1–100).

El archivo se carga al iniciar el motor y se actualiza tras cada partida si la opción
`Experience Readonly` está desactivada.

## License

 Wordfish is distributed under the **[GNU General Public License v3][gpl-link]** (GPLv3).
It integrates source code from:

- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Berserk](https://github.com/jhonnold/berserk/tree/main/src)
- [Obsidian](https://github.com/gab8192/Obsidian)

 Because these projects are GPLv3, any distribution of Wordfish must also comply with GPLv3.
When redistributing, you must:
1. Include the original license text (`COPYING.txt`)
2. Provide complete corresponding source code
3. Disclose all modifications under GPLv3

## Acknowledgements

 Wordfish also benefits from:
- Neural networks trained on [Lichess open database][lichess-db]
- Search techniques from [CCC testing community][ccc-link]
- Positional analysis concepts from [CPW research][cpw-link]

[gpl-link]: https://www.gnu.org/licenses/gpl-3.0.html
[doc-link]: #
[discord-link]: #
[openbench-link]: #
[worker-link]: #
[testsuite-link]: #
[discussions-link]: #
[chesswiki-link]: https://www.chessprogramming.org
[lichess-db]: https://database.lichess.org
[ccc-link]: https://www.chess.com/computer-chess-championship
[cpw-link]: https://www.chessprogramming.org
