/*
  Brainlearn, a UCI chess playing engine derived from Brainlearn
  Copyright (C) 2004-2025 A.Manzo, F.Ferraguti, K.Kiniama and Brainlearn developers (see AUTHORS file)

  Brainlearn is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Brainlearn is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mcts/brainlearn_mcts.h"

namespace Stockfish {

namespace BrainLearnMCTS {

void request_stop() { Brainlearn::mctsStopRequested.store(true, std::memory_order_relaxed); }

void clear_stop() { Brainlearn::mctsStopRequested.store(false, std::memory_order_relaxed); }

void clear() {
    clear_stop();
    Brainlearn::MCTS.clear();
}

}  // namespace BrainLearnMCTS

}  // namespace Stockfish
