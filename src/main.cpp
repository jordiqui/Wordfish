/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

  Modifications Copyright (C) 2024 Jorge Ruiz Centelles
*/

#include "misc.h"
#include "uci.h"
#include "tune.h"
#include "bitboard.h"
#include "position.h"

#ifndef ENGINE_NAME
    // override at build time with:  -DENGINE_NAME="\"Wordfish 2.42-190825\""
    #define ENGINE_NAME "Wordfish 2.42-190825"
#endif

using namespace Stockfish;

int main(int argc, char* argv[]) {

    // Some GUI front-ends interpret anything written to stderr as an engine
    // failure.  To avoid spurious error dialogs, only print the banner and
    // compiler information when command line arguments are supplied (typically
    // when running from a terminal for testing or benchmarking).
    if (argc > 1)
    {
        // Clear, consistent banner (many GUIs echo this to their logs).
        // Send banner to stderr so it doesn't interfere with UCI handshake on
        // stdout when run from a terminal.
        std::cerr << ENGINE_NAME
                  << " by Jorge Ruiz Centelles and the Stockfish developers (see AUTHORS file)"
                  << std::endl;

        std::cerr << compiler_info() << std::endl;
    }

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();
    return 0;
}
