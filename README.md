# Revolution Chess Engine

**Version 1.0**

<div align="center">
  <img src="[https://ijccrl.com/wp-content/uploads/2025/08/revolution.png]" 
  <h3>Revolution</h3>
  
  A free and open-source UCI chess engine combining classical algorithms with neural network innovations.
  <br>
  <strong><a href="#">Explore Revolution Documentation »</a>

  <em>Author: This distribution includes modifications and new code by Jorge Ruiz Centelles, with credit to ChatGPT, exploring new ideas.</em>
  
</div>

## Overview

**Revolution** is a free, open-source UCI chess engine derived from **Stockfish**. Jorge Ruiz Centelles, with credit to ChatGPT, modifies and extends the code to explore new concepts. The engine implements cutting-edge search algorithms combined with neural network evaluation. Derived from fundamental chess programming principles, Revolution analyzes positions through parallelized alpha-beta search enhanced with null-move pruning and late move reductions.

As a UCI-compliant engine, Revolution operates through **standard chess interfaces** without an integrated graphical interface. Users must employ compatible chess GUIs (Arena, Scid vs PC, etc.) for board visualization and move input. Consult your GUI documentation for implementation details.

## Technical Architecture

Revolution's architecture features:

- Hybrid evaluation system combining classical heuristics with NNUE networks
- SMP parallelization with YBWC (Young Brothers Wait Concept)
- Advanced pruning techniques (Reverse Futility Pruning, Late Move Pruning)
- Efficient move ordering with history heuristics and killer moves
- Optional root experience book storing previously played moves

## Files

The distribution includes:

- `README.md` (this documentation)
- `COPYING.txt` ([GNU GPLv3 license][gpl-link])
- `AUTHORS` (contributor acknowledgments)
- `src/` (source code with platform-specific Makefiles)
- Neural network weights (`revolution.nnue`)

## Contributing

### Development Guidelines
Contributions must adhere to:
- Clean, documented C++17 implementations
- Benchmark validation through perft testing
- Elo measurement via [OpenBench][openbench-link]
- Compatibility with UCI protocol standard

### Testing Infrastructure
Improvements require extensive testing:
- Install the [Revolution Test Worker][worker-link]
- Participate in active tests on [Revolution Test Suite][testsuite-link]
- Verify ELO gains through SPRT validation

### Community
Technical discussions occur primarily through:
- [Revolution Discord Server][discord-link]
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

Revolution can probe [Syzygy](https://github.com/syzygy1) endgame tablebases when a
directory is supplied via the `SyzygyPath` UCI option. The engine also exposes a
`SyzygyPremap` boolean option. When set to `true`, `Tablebases::init` pre-maps all
available WDL and DTZ tables during initialization, reducing probe latency at the
expense of additional startup time and memory usage.

## Experience Learning

Revolution includes a simple text-based cache that stores root moves and evaluations from
previous games. Rather than forcing book moves, the cached information biases root move
ordering during search. The following UCI options control this system:

- `Experience Enabled`: enables or disables the experience feature (default `true`).
- `Experience File`: name of the file where the experience data is stored (default `experience.exp`; legacy `.bin` files are converted automatically to this format).
- `Experience Readonly`: if `true`, no changes are written to the file.
- `Experience Prior`: uses stored experience to bias root move ordering.
- `Experience Width`: number of principal moves to consider (1–20).
- `Experience Eval Weight`: weighting of evaluation when ordering moves (0–10).
- `Experience Min Depth`: minimum depth required to store a move (4–64).
- `Experience Max Moves`: maximum number of moves saved per position (1–100).
- `Experience Book`: if `true`, use the experience file as an opening book.
- `Experience Book Max Moves`: limit of moves considered when using the experience book (1–100, default 100).
- `Experience Book Min Depth`: minimum depth required for a move to be used from the experience book (1–255, default 4).

The file is loaded at engine startup and updated after each game if `Experience Readonly` is disabled.

## UCI Options

### Minimum Thinking Time

The `Minimum Thinking Time` option ensures the engine spends at least a
specified number of milliseconds searching for a move, even if it finds
a good one instantly. This avoids extremely fast, low-quality replies.
Set it with:

```
setoption name Minimum Thinking Time value <milliseconds>
```

### Falcon Net

Revolution can switch to an alternative neural network using the
`FalconFile` option. To load the bundled `3.net` file, send:

```
setoption name FalconFile value 3.net
```

## License

Revolution is distributed under the **[GNU General Public License v3][gpl-link]** (GPLv3).
It integrates source code from:

- [Stockfish](https://github.com/official-stockfish/Stockfish)

Because Stockfish is GPLv3, any distribution of Revolution must also comply with GPLv3.
For a summary of your obligations under GPLv3 see <https://www.gnu.org/licenses/quick-guide-gplv3.html>.
When redistributing, you must:
1. Include the original license text (`COPYING.txt`)
2. Provide complete corresponding source code
3. Disclose all modifications under GPLv3

## Acknowledgements

Revolution also benefits from:
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
