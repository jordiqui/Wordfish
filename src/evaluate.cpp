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
*/

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <tuple>

#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"

namespace Stockfish {

// Returns a static, purely materialistic evaluation of the position from
// the point of view of the side to move. It can be divided by PawnValue to get
// an approximation of the material advantage on the board in terms of pawns.
int Eval::simple_eval(const Position& pos) {
    Color c = pos.side_to_move();
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c))
         + (pos.non_pawn_material(c) - pos.non_pawn_material(~c));
}

bool Eval::use_smallnet(const Position& pos) { return std::abs(simple_eval(pos)) > 962; }

namespace {

struct MaterialSummary {
    int totalPieces;
    int pawns;
    int majorPieces;
    int minorPieces;
    int materialImbalance;
};

MaterialSummary summarize_material(const Position& pos, int simpleEvalAbs) {
    MaterialSummary summary{};
    summary.totalPieces       = pos.count<ALL_PIECES>();
    summary.pawns             = pos.count<PAWN>();
    summary.majorPieces       = pos.count<ROOK>() + pos.count<QUEEN>();
    summary.minorPieces       = pos.count<BISHOP>() + pos.count<KNIGHT>();
    summary.materialImbalance = simpleEvalAbs;
    return summary;
}

bool should_use_falcon(const MaterialSummary& summary, bool smallNetPreferred) {
    const bool deepEndgame   = summary.totalPieces <= 7
                            || (summary.pawns <= 4 && summary.majorPieces <= 1);
    const bool lightMaterial = (summary.pawns <= 5 && summary.totalPieces <= 12)
                            || summary.minorPieces <= 1;
    const bool highImbalance = summary.materialImbalance > 800
                               && (summary.pawns <= 6 || summary.majorPieces <= 1);

    if (deepEndgame)
        return true;

    if (smallNetPreferred)
        return highImbalance || lightMaterial;

    return highImbalance && lightMaterial;
}

}  // namespace

// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
Value Eval::evaluate(const Eval::NNUE::Networks&    networks,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());

    const int  simpleEvalAbs    = std::abs(simple_eval(pos));
    const auto materialSummary = summarize_material(pos, simpleEvalAbs);

    bool  smallNetCandidate = materialSummary.materialImbalance > 962;
    bool  falconAvailable   = networks.falcon.is_available();
    bool  useFalconNet      = falconAvailable && should_use_falcon(materialSummary, smallNetCandidate);
    bool  smallNet          = smallNetCandidate;
    Value nnue              = VALUE_ZERO;
    Value psqt              = VALUE_ZERO;
    Value positional        = VALUE_ZERO;

    if (useFalconNet)
    {
        auto& falconCache = caches.cache_for_falcon(networks.falcon);
        auto  falconEval  = networks.falcon.evaluate(pos, accumulators, &falconCache);

        auto& bigCache = caches.cache_for_big(networks.big);
        auto  bigEval  = networks.big.evaluate(pos, accumulators, &bigCache);

        Value falconPsqt, falconPositional;
        Value bigPsqt, bigPositional;
        std::tie(falconPsqt, falconPositional) = falconEval;
        std::tie(bigPsqt, bigPositional)       = bigEval;

        int imbalance     = std::min(1500, materialSummary.materialImbalance);
        int scarcityBonus = std::max(0, 10 - materialSummary.totalPieces);
        int falconWeight  = 64 + imbalance * 32 / 1500 + scarcityBonus * 4;
        falconWeight      = std::clamp(falconWeight, 48, 120);

        psqt       = Value((falconWeight * falconPsqt + (128 - falconWeight) * bigPsqt) / 128);
        positional = Value((falconWeight * falconPositional + (128 - falconWeight) * bigPositional)
                           / 128);

        Value falconScore = (125 * falconPsqt + 131 * falconPositional) / 128;
        Value bigScore    = (125 * bigPsqt + 131 * bigPositional) / 128;
        nnue              = Value((falconWeight * falconScore + (128 - falconWeight) * bigScore) / 128);
        smallNet          = false;
    }
    else
    {
        if (smallNetCandidate)
        {
            auto& smallCache = caches.cache_for_small(networks.small);
            std::tie(psqt, positional) = networks.small.evaluate(pos, accumulators, &smallCache);
        }
        else
        {
            auto& bigCache = caches.cache_for_big(networks.big);
            std::tie(psqt, positional) = networks.big.evaluate(pos, accumulators, &bigCache);
            smallNet                   = false;
        }

        nnue = (125 * psqt + 131 * positional) / 128;
    }

    // Re-evaluate the position when higher eval accuracy is worth the time spent
    if (!useFalconNet && smallNet && (std::abs(nnue) < 236))
    {
        auto& bigCache = caches.cache_for_big(networks.big);
        std::tie(psqt, positional) = networks.big.evaluate(pos, accumulators, &bigCache);
        nnue                       = (125 * psqt + 131 * positional) / 128;
        smallNet                   = false;
    }

    // Blend optimism and eval with nnue complexity
    int nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / 468;
    nnue -= nnue * nnueComplexity / 18000;

    int material = 535 * materialSummary.pawns + pos.non_pawn_material();
    int v        = (nnue * (77777 + material) + optimism * (7777 + material)) / 77777;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 212;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Networks& networks) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    Eval::NNUE::AccumulatorStack accumulators;
    auto                         caches = std::make_unique<Eval::NNUE::AccumulatorCaches>(networks);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, networks, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    auto& bigCache = caches->cache_for_big(networks.big);
    auto [psqt, positional] = networks.big.evaluate(pos, accumulators, &bigCache);
    Value v                 = psqt + positional;
    v                       = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)\n";

    v = evaluate(networks, pos, accumulators, *caches, VALUE_ZERO);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "Final evaluation       " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]";
    ss << "\n";

    return ss.str();
}

namespace Eval {
void set_adaptive_style(bool) {}
}  // namespace Eval

}  // namespace Stockfish

