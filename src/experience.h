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
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Modifications Copyright (C) 2024 Jorge Ruiz Centelles
*/

#ifndef EXPERIENCE_H_INCLUDED
#define EXPERIENCE_H_INCLUDED

#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

#include "position.h"
#include "types.h"

namespace Stockfish {

struct ExperienceEntry {
    Move move;
    int  score;
    int  depth;
    int  count;
};

class Experience {
   public:
    void clear();
    void load(const std::string& file);
    void save(const std::string& file) const;
    Move probe(Position& pos, [[maybe_unused]] int width, int evalImportance,
               int minDepth, int maxMoves);
    void update(Position& pos, Move move, int score, int depth);
    void insert_entry(uint64_t key, uint16_t move, int value, int depth, int count);

    // Dirty / flush helpers
    void mark_dirty()        { dirty_.store(true, std::memory_order_relaxed); }
    void clear_dirty() const { dirty_.store(false, std::memory_order_relaxed); }
    bool dirty()       const { return dirty_.load(std::memory_order_relaxed); }

    static uint64_t compose_key(uint64_t posKey, uint16_t move16);

    mutable std::mutex mtx;

   private:
    std::unordered_map<Key, std::vector<ExperienceEntry>> table;
    mutable std::atomic<bool> dirty_{false};
};

extern Experience experience;

}  // namespace Stockfish

#endif  // EXPERIENCE_H_INCLUDED
