/*
  Wordfish, a UCI chess engine based on Stockfish, Berserk, and Obsidian
  Copyright (C) 2024 Jorge Ruiz Centelles

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Portions of this file are adapted from Stockfish and retain their
  original licensing.
*/

#include "misc.h"
#include "uci.h"
#include "tune.h"
#include "bitboard.h"
#include "position.h"

#ifndef ENGINE_BUILD_DATE
// Fallback to compilation date if not provided by the build system
#define ENGINE_BUILD_DATE __DATE__
#endif

#ifndef ENGINE_NAME
// Override at build time with:  -DENGINE_NAME="\"Wordfish 1.0.1 dev\""
#define ENGINE_NAME "Wordfish 1.0.1 dev"
#endif

using namespace Stockfish;

int main(int argc, char* argv[]) {

    // Clear, consistent banner (many GUIs echo this to their logs)
    std::cout << ENGINE_NAME << ' ' << ENGINE_BUILD_DATE << ' ' << __TIME__
              << " by Jorge Ruiz Centelles" << std::endl;

    std::cout << compiler_info() << std::endl;

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();
    return 0;
}