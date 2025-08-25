/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#include "misc.h"
#include "uci.h"
#include "tune.h"
#include "bitboard.h"
#include "position.h"

#ifndef ENGINE_BUILD_DATE
// yymmdd; override at build time with:  -DENGINE_BUILD_DATE=250822
#define ENGINE_BUILD_DATE "220825"
#endif

#ifndef ENGINE_NAME
// override at build time with:  -DENGINE_NAME="\"Wordfish 1.0.1 dev\""
#define ENGINE_NAME "Wordfish 1.0.1 dev"
#endif

using namespace Stockfish;

int main(int argc, char* argv[]) {

    // Clear, consistent banner (many GUIs echo this to their logs)
    std::cout << ENGINE_NAME << ' ' << ENGINE_BUILD_DATE
              << ' ' << __DATE__ << ' ' << __TIME__
              << " by Jorge Ruiz" << std::endl;

    std::cout << compiler_info() << std::endl;

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();
    return 0;
}