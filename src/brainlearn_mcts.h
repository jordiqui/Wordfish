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

#ifndef BRAINLEARN_MCTS_H_INCLUDED
#define BRAINLEARN_MCTS_H_INCLUDED

#include <array>
#include <atomic>
#include <cassert>
#include <thread>
#include <unordered_map>

#include "misc.h"
#include "movepick.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"

namespace Stockfish::BrainLearnMCTS {

using Reward = double;

constexpr Reward REWARD_NONE  = 0.0;
constexpr Reward REWARD_MATED = 0.0;
constexpr Reward REWARD_DRAW  = 0.5;
constexpr Reward REWARD_MATE  = 1.0;

constexpr Value VALUE_KNOWN_WIN = 10000;

enum EdgeStatistic {
    STAT_UCB,
    STAT_VISITS,
    STAT_MEAN,
    STAT_PRIOR
};

struct Edge {
    Edge() :
        move(Move::none()),
        visits(0),
        prior(REWARD_NONE),
        actionValue(REWARD_NONE),
        meanActionValue(REWARD_NONE) {}

    Edge(const Edge&)            = delete;
    Edge& operator=(const Edge&) = delete;

    std::atomic<Move>   move;
    std::atomic<double> visits;
    std::atomic<Reward> prior;
    std::atomic<Reward> actionValue;
    std::atomic<Reward> meanActionValue;
};

extern size_t mctsThreads;
extern size_t mctsMultiStrategy;
extern double mctsMultiMinVisits;

constexpr int MAX_CHILDREN = MAX_MOVES;
using EdgeArray            = std::array<Edge*, MAX_CHILDREN>;

class Spinlock {
    static const size_t NO_THREAD = 0;

    std::atomic<size_t> owner;
    int                 lockCount;

   public:
    Spinlock() :
        owner(0),
        lockCount(0) {}

    Spinlock(const Spinlock&)            = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void acquire(size_t threadId) {
        if (mctsThreads > 1)
        {
            size_t currentOwner = NO_THREAD;

            while (!owner.compare_exchange_weak(currentOwner, threadId, std::memory_order_acquire,
                                                std::memory_order_relaxed)
                   && currentOwner != threadId)
            {
                currentOwner = NO_THREAD;
                std::this_thread::yield();
            }

            lockCount++;
        }
    }

    void release([[maybe_unused]] size_t threadId) {
        if (mctsThreads > 1)
        {
            assert(owner.load(std::memory_order_relaxed) == threadId);

            if (--lockCount == 0)
                owner.store(0);
        }
    }
};

struct mctsNodeInfo {
    mctsNodeInfo() {
        for (size_t i = 0; i < children.size(); ++i)
            children[i] = new Edge();
    }

    ~mctsNodeInfo() {
        for (size_t i = 0; i < children.size(); ++i)
            delete children[i];
    }

    mctsNodeInfo(const mctsNodeInfo&)            = delete;
    mctsNodeInfo& operator=(const mctsNodeInfo&) = delete;

    Spinlock lock;

    Key                key1           = 0;
    Key                key2           = 0;
    std::atomic<long>  node_visits    = 0;
    std::atomic<int>   number_of_sons = 0;
    std::atomic<Move>  lastMove       = Move::none();
    std::atomic<Value> ttValue        = VALUE_NONE;
    std::atomic<bool>  AB             = false;
    EdgeArray          children;
};

class MonteCarlo;

mctsNodeInfo* get_node(const MonteCarlo* mcts, const Position& pos);

using MCTS_MAP_BASE = std::unordered_multimap<Key, mctsNodeInfo*>;

extern std::atomic<size_t> MCTSNodeCount;

class MCTSHashTable: public MCTS_MAP_BASE {
   public:
    ~MCTSHashTable() { clear(); }

    void clear() {
        for (unsigned i = 0; i < bucket_count(); ++i)
        {
            for (auto it = begin(i); it != end(i); ++it)
                delete it->second;
        }

        MCTS_MAP_BASE::clear();
        MCTSNodeCount = 0;
    }
};

extern MCTSHashTable MCTS;

const size_t MCTSMaxNodes = 100000;

class MonteCarlo {
    friend class AutoSpinLock;

   public:
    MonteCarlo(Position& p, Search::Worker* worker, TranspositionTable& transpositionTable);

    MonteCarlo(const MonteCarlo&)            = delete;
    MonteCarlo& operator=(const MonteCarlo&) = delete;

    void search(ThreadPool& threads, Search::LimitsType limits, bool isMainThread,
                Search::Worker* worker);

    void          create_root(Search::Worker* worker);
    bool          computational_budget(ThreadPool& threads, Search::LimitsType limits);
    mctsNodeInfo* tree_policy(ThreadPool& threads, Search::LimitsType limits);
    Reward        playout_policy(mctsNodeInfo* node);
    Value         backup(Reward r, bool AB_Mode);
    Edge*         best_child(mctsNodeInfo* node, EdgeStatistic statistic) const;

    double ucb(const Edge* edge, long fatherVisits, bool priorMode) const;

    bool is_root(const mctsNodeInfo* node) const;
    bool is_terminal(mctsNodeInfo* node) const;
    void do_move(Move m);
    void undo_move();
    void generate_moves(mctsNodeInfo* node);

    [[nodiscard]] Reward value_to_reward(Value v) const;
    [[nodiscard]] Value  reward_to_value(Reward r) const;
    [[nodiscard]] Value  evaluate_with_minimax(Depth d) const;
    Value                evaluate_with_minimax(mctsNodeInfo* node, Depth d) const;
    [[nodiscard]] Reward evaluate_terminal(mctsNodeInfo* node) const;
    Reward               calculate_prior(Move m);
    void                 add_prior_to_node(mctsNodeInfo* node, Move m, Reward prior) const;

    void                 default_parameters();
    void                 set_exploration_constant(double c);
    [[nodiscard]] double exploration_constant() const;

    [[nodiscard]] bool should_emit_pv(bool isMainThread) const;
    void               emit_pv(Search::Worker* worker, ThreadPool& threads);
    void               print_children();

    int max_ply() const { return maximumPly; }

   private:
    Position&       pos;
    Search::Worker* thisThread;
    TranspositionTable& tt;
    mctsNodeInfo* root{};

    int       ply{};
    int       maximumPly{};
    TimePoint startTime{};
    TimePoint lastOutputTime{};

    [[maybe_unused]] double max_epsilon = 0.99;
    [[maybe_unused]] double min_epsilon = 0.00;
    [[maybe_unused]] double decay_rate  = 0.8;
    bool                    AB_Rollout{};

    double BACKUP_MINIMAX{};
    double UCB_UNEXPANDED_NODE{};
    double UCB_EXPLORATION_CONSTANT{};
    double UCB_LOSSES_AVOIDANCE{};
    double UCB_LOG_TERM_FACTOR{};
    bool   UCB_USE_FATHER_VISITS{};
    int    PRIOR_FAST_EVAL_DEPTH{};
    int    PRIOR_SLOW_EVAL_DEPTH{};

    mctsNodeInfo *nodesBuffer[MAX_PLY + 10]{}, **nodes  = nodesBuffer + 7;
    Edge *        edgesBuffer[MAX_PLY + 10]{}, **edges  = edgesBuffer + 7;
    Search::Stack stackBuffer[MAX_PLY + 17]{}, *stack   = stackBuffer + 7;
    StateInfo     statesBuffer[MAX_PLY + 10]{}, *states = statesBuffer + 7;
};

void clear();

}  // namespace Stockfish::BrainLearnMCTS

#endif  // BRAINLEARN_MCTS_H_INCLUDED
