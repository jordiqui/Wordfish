#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "misc.h"
#include "types.h"

namespace Stockfish {

class OptionsMap;
class Position;

namespace Search {
struct LimitsType;
struct RootMove;
}

namespace MCTS {

struct MoveStats {
    Move           move;
    double         averageValue;
    std::uint64_t  visits;
};

struct Result {
    Move                          bestMove;
    std::vector<Move>             principalVariation;
    Value                         evaluation;
    std::vector<MoveStats>        moveStats;
    std::uint64_t                 simulations;
    std::uint64_t                 nodesVisited;
    TimePoint                     elapsedMs;
};

Result analyze(Position&                    rootPos,
               Color                        rootColor,
               const OptionsMap&            options,
               const Search::LimitsType&    limits,
               std::function<bool()>        stopRequested,
               int                          rolloutDepthHint = 12);

}  // namespace MCTS

}  // namespace Stockfish

