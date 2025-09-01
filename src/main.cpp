/*
  Wordfish 2.0 dev, a UCI chess engine based on Stockfish, Berserk, and Obsidian
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)
  Copyright (C) 2024 Jorge Ruiz Centelles
  Credits: ChatGPT

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
#define ENGINE_BUILD_DATE "010925"
#endif

#ifndef ENGINE_NAME
#define ENGINE_NAME "Wordfish 2.0 dev"
#endif

using namespace Stockfish;

int main(int argc, char* argv[]) {

    // Clear, consistent banner (many GUIs echo this to their logs)
    std::cout << ENGINE_NAME << ' ' << ENGINE_BUILD_DATE << ' ' << __TIME__
              << " by Stockfish developers, Jorge Ruiz Centelles and ChatGPT"
              << std::endl;

    std::cout << compiler_info() << std::endl;

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();
    return 0;
}