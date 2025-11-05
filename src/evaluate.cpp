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
#include <array>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <tuple>

#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "bitboard.h"
#include "misc.h"
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

enum class WarmupSlot : std::size_t {
    Big,
    Small,
    Count
};

using WarmupDurations = std::array<double, static_cast<std::size_t>(WarmupSlot::Count)>;

struct WarmupProfile {
    std::atomic<int>         basePly{std::numeric_limits<int>::min()};
    std::atomic<std::uint32_t> seenMask{0};
    std::atomic<bool>          active{false};
} warmupProfile;

void log_warmup_sample(int relPly, const WarmupDurations& durations) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "NNUE warmup ply " << (relPly + 1)
        << " big=" << durations[static_cast<std::size_t>(WarmupSlot::Big)] << "ms"
        << " small=" << durations[static_cast<std::size_t>(WarmupSlot::Small)] << "ms";

    sync_cout_start();
    std::cout << "info string " << oss.str() << '\n';
    sync_cout_end();
}

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

}  // namespace

constexpr Bitboard DarkSquaresMask = 0xAA55AA55AA55AA55ULL;

namespace {

bool rooks_connected_home(const Position& pos, Color c) {
    if (pos.count<ROOK>(c) < 2)
        return false;

    Bitboard rooks    = pos.pieces(c, ROOK);
    Bitboard homeRank = rank_bb(relative_rank(c, RANK_1));
    Bitboard homeRooks = rooks & homeRank;

    if (popcount(homeRooks) < 2)
        return false;

    Square left  = lsb(homeRooks);
    Square right = msb(homeRooks);

    Bitboard between = between_bb(left, right) ^ square_bb(right);
    return !(between & pos.pieces());
}

int rook_connection_bonus(const Position& pos, Color c) {
    if (pos.count<ROOK>(c) < 2)
        return 0;

    int      bonus       = 0;
    Bitboard rooks       = pos.pieces(c, ROOK);
    Bitboard connected   = rooks_connected_home(pos, c) ? rooks : Bitboard(0);
    Bitboard liftedRanks = c == WHITE ? (rank_bb(RANK_3) | rank_bb(RANK_4))
                                      : (rank_bb(RANK_6) | rank_bb(RANK_5));

    if (connected)
        bonus += 18;

    Bitboard lifted = rooks & liftedRanks;
    while (lifted)
    {
        Square rsq = pop_lsb(lifted);
        if (!(pos.pieces(c, PAWN) & file_bb(file_of(rsq))))
            bonus += 5;
    }

    return bonus;
}

int king_open_file_penalty(const Position& pos, Color c) {
    const Square kingSq  = pos.square<KING>(c);
    const File   kingFile = file_of(kingSq);

    int penalty = 0;

    for (int df = -1; df <= 1; ++df)
    {
        File f = File(int(kingFile) + df);
        if (f < FILE_A || f > FILE_H)
            continue;

        Bitboard friendly = pos.pieces(c, PAWN) & file_bb(f);
        Bitboard enemy    = pos.pieces(~c, PAWN) & file_bb(f);

        if (!friendly)
        {
            penalty += enemy ? 16 : 20;
            if (pos.pieces(~c, ROOK, QUEEN) & file_bb(f))
                penalty += 8;
        }
        else
        {
            Square guardSq = c == WHITE ? msb(friendly) : lsb(friendly);
            Rank   relRank = relative_rank(c, guardSq);

            if (relRank >= RANK_4)
                penalty += 10 + 2 * (int(relRank) - int(RANK_3));
        }
    }

    return penalty;
}

int king_dark_square_penalty(const Position& pos, Color c) {
    const Square kingSq    = pos.square<KING>(c);
    const bool   kingOnDark = ((rank_of(kingSq) + file_of(kingSq)) % 2) == 0;

    Bitboard bishops = pos.pieces(c, BISHOP);
    bool     bishopCover = kingOnDark ? (bishops & DarkSquaresMask)
                                      : (bishops & ~DarkSquaresMask);

    if (bishopCover)
        return 0;

    int        penalty = 0;
    const int  forward = c == WHITE ? 8 : -8;

    for (int df = -1; df <= 1; ++df)
    {
        Square shieldSq = Square(int(kingSq) + forward + df);
        if (!is_ok(shieldSq))
            continue;

        const bool sameColor = ((rank_of(shieldSq) + file_of(shieldSq)) % 2)
                               == (kingOnDark ? 0 : 1);
        if (!sameColor)
            continue;

        Piece shield = pos.piece_on(shieldSq);
        if (shield == NO_PIECE)
            penalty += 7;
        else if (color_of(shield) == ~c)
            penalty += 5;
        else if (type_of(shield) == PAWN && relative_rank(c, shieldSq) >= RANK_4)
            penalty += 6;
    }

    return penalty;
}

int flank_storm_penalty(const Position& pos, Color c) {
    static constexpr File flankFiles[] = {FILE_A, FILE_B, FILE_G, FILE_H};

    int      penalty      = 0;
    Bitboard pawns        = pos.pieces(c, PAWN);
    const bool rooksReady = rooks_connected_home(pos, c);

    for (File f : flankFiles)
    {
        Bitboard filePawns = pawns & file_bb(f);
        if (!filePawns)
            continue;

        Square front = c == WHITE ? msb(filePawns) : lsb(filePawns);
        Rank   rel   = relative_rank(c, front);

        if (rel >= RANK_4)
        {
            int base = (f == FILE_A || f == FILE_H) ? 14 : 10;
            base += 2 * std::max(0, int(rel) - int(RANK_4));

            if (!rooksReady)
                base += 8;

            penalty += base;
        }
    }

    return penalty;
}

}  // namespace

// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
Value Eval::evaluate(const Eval::NNUE::Networks&    networks,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism,
                     Depth                          searchDepth) {

    assert(!pos.checkers());

    using Clock = std::chrono::steady_clock;

    std::array<double, static_cast<std::size_t>(WarmupSlot::Count)> warmupDurations{};
    std::uint32_t warmupMask  = 0;
    bool          warmupLog   = false;
    int           warmupRelPly = -1;

    if (warmupProfile.active.load(std::memory_order_relaxed))
    {
        int base = warmupProfile.basePly.load(std::memory_order_relaxed);
        if (base != std::numeric_limits<int>::min())
        {
            int rel = pos.game_ply() - base;
            if (rel >= 0 && rel < 5)
            {
                warmupMask  = 1u << rel;
                warmupRelPly = rel;
                if (!(warmupProfile.seenMask.load(std::memory_order_relaxed) & warmupMask))
                    warmupLog = true;
            }
        }
    }

    auto time_call = [&](auto&& fn, WarmupSlot slot) {
        if (!warmupLog)
            return fn();

        auto start   = Clock::now();
        auto result  = fn();
        auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        warmupDurations[static_cast<std::size_t>(slot)] += elapsed;
        return result;
    };

    const int  simpleEvalAbs    = std::abs(simple_eval(pos));
    const auto materialSummary = summarize_material(pos, simpleEvalAbs);

    bool  smallNetCandidate = materialSummary.materialImbalance > 962;
    bool  smallNet          = smallNetCandidate;
    Value nnue              = VALUE_ZERO;
    Value psqt              = VALUE_ZERO;
    Value positional        = VALUE_ZERO;

    if (smallNetCandidate)
    {
        auto& smallCache = caches.cache_for_small(networks.small);
        auto  smallEval  = time_call(
          [&] { return networks.small.evaluate(pos, accumulators, &smallCache); }, WarmupSlot::Small);
        std::tie(psqt, positional) = smallEval;
    }
    else
    {
        auto& bigCache = caches.cache_for_big(networks.big);
        auto  bigEval  = time_call(
          [&] { return networks.big.evaluate(pos, accumulators, &bigCache); }, WarmupSlot::Big);
        std::tie(psqt, positional) = bigEval;
        smallNet                   = false;
    }

    nnue = (125 * psqt + 131 * positional) / 128;

    // Re-evaluate the position when higher eval accuracy is worth the time spent
    if (smallNet && (std::abs(nnue) < 236))
    {
        auto& bigCache = caches.cache_for_big(networks.big);
        auto  bigEval  = time_call(
          [&] { return networks.big.evaluate(pos, accumulators, &bigCache); }, WarmupSlot::Big);
        std::tie(psqt, positional) = bigEval;
        nnue                       = (125 * psqt + 131 * positional) / 128;
        smallNet                   = false;
    }

    // Blend optimism and eval with nnue complexity
    int nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / 468;
    nnue -= nnue * nnueComplexity / 18000;

    int material = 535 * materialSummary.pawns + pos.non_pawn_material();
    int v        = (nnue * (77777 + material) + optimism * (7777 + material)) / 77777;

    const Color us   = pos.side_to_move();
    const Color them = ~us;

    int ourStormPenalty = flank_storm_penalty(pos, us);
    if (ourStormPenalty && searchDepth < 14)
    {
        int depthShortfall = std::max(0, 14 - std::max<int>(0, searchDepth));
        int scale          = 128 + depthShortfall * 12;
        ourStormPenalty    = ourStormPenalty * scale / 128;
    }

    int ourPenalty = king_open_file_penalty(pos, us) + king_dark_square_penalty(pos, us)
                   + ourStormPenalty;
    int theirPenalty = king_open_file_penalty(pos, them) + king_dark_square_penalty(pos, them)
                     + flank_storm_penalty(pos, them);

    int safetySwing = theirPenalty - ourPenalty;
    int rookDiff    = rook_connection_bonus(pos, us) - rook_connection_bonus(pos, them);

    v += safetySwing + rookDiff;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 212;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    if (warmupLog && warmupMask)
    {
        std::uint32_t previous = warmupProfile.seenMask.fetch_or(warmupMask, std::memory_order_acq_rel);
        if ((previous & warmupMask) == 0 && warmupRelPly >= 0)
        {
            log_warmup_sample(warmupRelPly, warmupDurations);
            if ((previous | warmupMask) == ((1u << 5) - 1))
                warmupProfile.active.store(false, std::memory_order_relaxed);
        }
    }

    return v;
}

void Eval::notify_new_fen(int gamePly) {
    warmupProfile.seenMask.store(0, std::memory_order_relaxed);
    warmupProfile.basePly.store(gamePly, std::memory_order_relaxed);
    warmupProfile.active.store(true, std::memory_order_relaxed);
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

    v = evaluate(networks, pos, accumulators, *caches, VALUE_ZERO, Depth(99));
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

