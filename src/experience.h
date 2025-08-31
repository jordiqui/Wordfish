#ifndef EXPERIENCE_H_INCLUDED
#define EXPERIENCE_H_INCLUDED

#include <unordered_map>
#include <vector>
#include <string>

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

   private:
    std::unordered_map<Key, std::vector<ExperienceEntry>> table;
};

extern Experience experience;

}  // namespace Stockfish

#endif  // EXPERIENCE_H_INCLUDED
