#include "mcts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "ucioption.h"

namespace Stockfish {
namespace MCTS {

namespace {

struct Node {
    Move                                move = Move::none();
    Color                               toMove = WHITE;
    bool                                terminal = false;
    Value                               terminalValue = VALUE_ZERO;
    std::vector<Move>                   unexpandedMoves;
    std::vector<std::unique_ptr<Node>>  children;
    double                              totalValue = 0.0;
    std::uint64_t                       visits     = 0;
};

std::vector<Move> generate_moves(const Position& pos, PRNG& rng) {
    std::vector<Move> moves;
    moves.reserve(MoveList<LEGAL>(pos).size());

    for (Move m : MoveList<LEGAL>(pos))
        moves.push_back(m);

    for (size_t i = moves.size(); i > 1; --i)
    {
        const size_t j = size_t(rng.rand<std::uint32_t>()) % i;
        std::swap(moves[i - 1], moves[j]);
    }

    return moves;
}

Value terminal_evaluation(const Position& pos) {
    if (pos.checkers())
        return VALUE_MATED_IN_MAX_PLY;

    return VALUE_DRAW;
}

double value_to_double(Value v) {
    return double(v) / PawnValue;
}

Value double_to_value(double v) {
    const double clamped = std::clamp(v, -double(VALUE_MATE), double(VALUE_MATE));
    const int    scaled  = int(std::round(clamped * PawnValue));
    return Value(std::clamp(scaled, -VALUE_MATE, VALUE_MATE));
}

struct Budget {
    std::uint64_t maxIterations;
    TimePoint     endTime;
    bool          useTime;
};

Budget compute_budget(const OptionsMap& options,
                      const Search::LimitsType& limits,
                      Color rootColor) {
    Budget budget{};

    const int defaultPlayouts = options.count("MCTS Simulations") ? int(options["MCTS Simulations"]) : 5000;

    budget.maxIterations = limits.nodes ? limits.nodes : defaultPlayouts;
    budget.useTime       = false;
    budget.endTime       = 0;

    if (limits.infinite)
        return budget;

    const TimePoint start       = now();
    const int       overhead    = int(options["Move Overhead"]);
    const int       minThinking = int(options["Minimum Thinking Time"]);

    auto compute_available_time = [&](TimePoint time, TimePoint inc, int movesToGo) {
        if (!time)
            return TimePoint(0);

        TimePoint available = time;
        if (inc)
            available += inc / 2;

        int divisor = movesToGo ? movesToGo : 30;
        available   = std::max(TimePoint(0), available - overhead);
        available   = available / divisor;
        available   = std::max(available, TimePoint(minThinking));
        return available;
    };

    if (limits.movetime)
    {
        TimePoint available = std::max(TimePoint(0), limits.movetime - overhead);

        if (available)
        {
            budget.useTime = true;
            budget.endTime = start + available;
        }
    }
    else if (limits.time[rootColor])
    {
        const TimePoint available = compute_available_time(limits.time[rootColor], limits.inc[rootColor],
                                                           limits.movestogo);
        if (available)
        {
            budget.useTime = true;
            budget.endTime = start + available;
        }
    }

    return budget;
}

std::optional<double> rollout(Position& pos,
                              PRNG& rng,
                              std::array<StateInfo, MAX_PLY>& stateStack,
                              int depthOffset,
                              int maxDepth,
                              const Budget& budget,
                              const std::function<bool()>& stopRequested) {
    std::vector<Move> played;
    played.reserve(maxDepth);

    for (int depth = 0; depth < maxDepth; ++depth)
    {
        if (stopRequested())
        {
            for (int i = depth - 1; i >= 0; --i)
                pos.undo_move(played[i]);
            return std::nullopt;
        }

        if (budget.useTime && now() >= budget.endTime)
        {
            for (int i = depth - 1; i >= 0; --i)
                pos.undo_move(played[i]);
            return std::nullopt;
        }

        if (pos.is_draw(0))
        {
            for (int i = depth - 1; i >= 0; --i)
                pos.undo_move(played[i]);
            return value_to_double(VALUE_DRAW);
        }

        MoveList<LEGAL> legal(pos);
        if (legal.size() == 0)
        {
            Value terminal = pos.checkers() ? VALUE_MATED_IN_MAX_PLY : VALUE_DRAW;
            for (int i = depth - 1; i >= 0; --i)
                pos.undo_move(played[i]);
            return value_to_double(terminal);
        }

        const size_t index = size_t(rng.rand<std::uint32_t>()) % legal.size();
        const Move   move  = *(legal.begin() + index);
        StateInfo& st   = stateStack[depthOffset + depth];
        pos.do_move(move, st);
        played.push_back(move);
    }

    Value eval = Value(Eval::simple_eval(pos));

    for (int i = int(played.size()) - 1; i >= 0; --i)
        pos.undo_move(played[i]);

    return value_to_double(eval);
}

Node* best_child_for_player(Node& node) {
    Node*  best      = nullptr;
    double bestScore = -std::numeric_limits<double>::infinity();

    for (auto& childPtr : node.children)
    {
        Node* child = childPtr.get();
        if (!child->visits)
            continue;

        const double mean = child->totalValue / std::max<std::uint64_t>(1, child->visits);
        const double score = -mean;

        if (score > bestScore)
        {
            bestScore = score;
            best      = child;
        }
    }

    return best;
}

}  // namespace

Result analyze(Position&                 rootPos,
               Color                     rootColor,
               const OptionsMap&         options,
               const Search::LimitsType& limits,
               std::function<bool()>     stopRequested,
               int                       rolloutDepthHint) {
    Result result{};

    PRNG rng(std::max<uint64_t>(1, uint64_t(now()) | 1));

    Budget budget = compute_budget(options, limits, rootColor);

    const int rolloutDepth = std::clamp(rolloutDepthHint, 4, 64);

    Position& pos = rootPos;

    Node root;
    root.move          = Move::none();
    root.toMove        = rootColor;
    root.unexpandedMoves = generate_moves(pos, rng);
    root.terminal      = root.unexpandedMoves.empty();
    if (root.terminal)
        root.terminalValue = terminal_evaluation(pos);

    std::array<StateInfo, MAX_PLY> stateStack{};

    const int exploreSetting = options.count("MCTS Explore") ? int(options["MCTS Explore"]) : 35;
    const double explorationBase = 1.41421356237;
    const double exploration     = (std::max(1, exploreSetting) / 35.0) * explorationBase;

    std::uint64_t iterations = 0;
    const TimePoint start    = now();

    while (!root.terminal)
    {
        if (stopRequested())
            break;

        if (budget.useTime && now() >= budget.endTime)
            break;

        if (budget.maxIterations && iterations >= budget.maxIterations)
            break;

        std::vector<Node*> nodeStack;
        nodeStack.reserve(rolloutDepth + 1);
        nodeStack.push_back(&root);

        std::vector<Move> moveStack;
        moveStack.reserve(rolloutDepth + 1);

        Position& current = pos;
        int      depth   = 0;

        while (true)
        {
            Node* node = nodeStack.back();

            if (node->terminal)
                break;

            if (!node->unexpandedMoves.empty())
            {
                const Move move = node->unexpandedMoves.back();
                node->unexpandedMoves.pop_back();

                StateInfo& st = stateStack[depth];
                current.do_move(move, st);
                moveStack.push_back(move);
                depth++;

                auto child       = std::make_unique<Node>();
                child->move      = move;
                child->toMove    = current.side_to_move();
                child->unexpandedMoves = generate_moves(current, rng);
                child->terminal  = child->unexpandedMoves.empty();
                if (child->terminal)
                    child->terminalValue = terminal_evaluation(current);

                Node* childPtr = child.get();
                node->children.push_back(std::move(child));
                nodeStack.push_back(childPtr);
                break;
            }

            if (node->children.empty())
                break;

            const double logVisits = std::log(std::max<std::uint64_t>(1, node->visits));
            Node*        bestChild = nullptr;
            double       bestScore = -std::numeric_limits<double>::infinity();

            for (auto& childPtr : node->children)
            {
                Node* child = childPtr.get();
                const double mean = child->visits ? child->totalValue / child->visits : 0.0;
                const double exploitation = -mean;
                const double explorationTerm = exploration
                                               * std::sqrt(logVisits
                                                           / (child->visits + 1.0));
                const double score = exploitation + explorationTerm;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestChild = child;
                }
            }

            if (!bestChild)
                break;

            StateInfo& st = stateStack[depth];
            current.do_move(bestChild->move, st);
            moveStack.push_back(bestChild->move);
            depth++;
            nodeStack.push_back(bestChild);
        }

        Node* leaf = nodeStack.back();

        double evaluation = 0.0;
        bool   aborted = false;
        if (leaf->terminal)
            evaluation = value_to_double(leaf->terminalValue);
        else
        {
            std::optional<double> rolloutEval = rollout(current, rng, stateStack, depth,
                                                        rolloutDepth - depth, budget, stopRequested);
            if (!rolloutEval)
                aborted = true;
            else
                evaluation = *rolloutEval;
        }

        if (aborted)
        {
            for (int i = int(moveStack.size()) - 1; i >= 0; --i)
                current.undo_move(moveStack[size_t(i)]);
            break;
        }

        for (int i = int(nodeStack.size()) - 1; i >= 0; --i)
        {
            Node* node = nodeStack[size_t(i)];
            node->visits++;
            node->totalValue += evaluation;
            evaluation = -evaluation;
        }

        for (int i = int(moveStack.size()) - 1; i >= 0; --i)
            current.undo_move(moveStack[size_t(i)]);

        ++iterations;
    }

    result.simulations = iterations;
    result.elapsedMs   = now() - start;

    if (root.visits)
    {
        const double mean = root.totalValue / root.visits;
        result.evaluation = double_to_value(mean);
    }
    else
        result.evaluation = VALUE_ZERO;

    result.moveStats.reserve(root.children.size());

    Node* best = nullptr;
    double bestScore = -std::numeric_limits<double>::infinity();

    for (auto& childPtr : root.children)
    {
        Node* child = childPtr.get();
        const double mean   = child->visits ? child->totalValue / child->visits : 0.0;
        const double score  = -mean;
        result.moveStats.push_back({child->move, score, child->visits});
        if (score > bestScore)
        {
            bestScore = score;
            best      = child;
        }
    }

    if (best)
    {
        result.bestMove = best->move;
        result.principalVariation.push_back(best->move);

        Node* current = best;
        int   depth   = 1;
        while (current && depth < rolloutDepth)
        {
            current = best_child_for_player(*current);
            if (!current)
                break;
            result.principalVariation.push_back(current->move);
            ++depth;
        }
    }

    if (result.principalVariation.empty())
        result.principalVariation.push_back(Move::none());

    return result;
}

}  // namespace MCTS
}  // namespace Stockfish

