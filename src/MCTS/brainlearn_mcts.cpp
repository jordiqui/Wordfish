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

#include "MCTS/brainlearn_mcts.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "misc.h"
#include "syzygy/tbprobe.h"
#include "uci.h"

namespace Stockfish::BrainLearnMCTS {

inline bool comp_float(const double a, const double b, const double epsilon = 0.005) {
    return std::fabs(a - b) < epsilon;
}

struct COMPARE_PRIOR {
    inline bool operator()(const Edge* a, const Edge* b) const { return a->prior > b->prior; }
} ComparePrior;

struct COMPARE_VISITS {
    inline bool operator()(const Edge* a, const Edge* b) const {
        return a->visits > b->visits
            || (comp_float(a->visits, b->visits, 0.005) && a->prior > b->prior);
    }
} CompareVisits;

struct COMPARE_MEAN_ACTION {
    inline bool operator()(const Edge* a, const Edge* b) const {
        return a->meanActionValue > b->meanActionValue;
    }
} CompareMeanAction;

struct COMPARE_ROBUST_CHOICE {
    inline bool operator()(const Edge* a, const Edge* b) const {
        return (10 * a->visits + a->prior > 10 * b->visits + b->prior);
    }
} CompareRobustChoice;

class AutoSpinLock {
   private:
    const MonteCarlo* _mcts;
    Spinlock&         _sl;
    size_t            _threadIndex;

   public:
    AutoSpinLock(const MonteCarlo* mcts, mctsNodeInfo* node) :
        AutoSpinLock(mcts, node->lock) {}

    AutoSpinLock(const MonteCarlo* mcts, Spinlock& sl) :
        _mcts(mcts),
        _sl(sl),
        _threadIndex(
          mctsThreads > 0 ? std::min(_mcts->thisThread->thread_index(), mctsThreads - 1) : 0) {
        _sl.acquire(_threadIndex);
    }

    ~AutoSpinLock() { _sl.release(_threadIndex); }
};

#define LOCK__(m, n, l) AutoSpinLock asl##l(m, n)
#define LOCK_(m, n, l) LOCK__(m, n, l)
#define LOCK(m, n) LOCK_(m, n, __LINE__)

MCTSHashTable       MCTS;
Edge                EDGE_NONE;
Spinlock            createLock;
size_t              mctsThreads;
size_t              mctsMultiStrategy;
double              mctsMultiMinVisits;
std::atomic<size_t> MCTSNodeCount(0);
std::atomic_bool    StopRequested(false);

void request_stop() { StopRequested.store(true, std::memory_order_relaxed); }

void clear_stop() { StopRequested.store(false, std::memory_order_relaxed); }

bool stop_requested() { return StopRequested.load(std::memory_order_relaxed); }

template<typename T>
T TRand(const T min, const T max) {
    static std::random_device        rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<T> distribution(min, max);

    return distribution(gen);
}

mctsNodeInfo* get_node(const MonteCarlo* mcts, const Position& p) {

    Key           key1 = p.key();
    Key           key2 = p.pawn_key();
    mctsNodeInfo* node = nullptr;

    LOCK(mcts, createLock);
    if (MCTSNodeCount.load(std::memory_order_relaxed) >= MCTSMaxNodes)
    {
        return nullptr;
    }
    const auto [fst, snd] = MCTS.equal_range(key1);
    auto       it1        = fst;
    const auto it2        = snd;
    while (it1 != it2)
    {
        node = *(&it1->second);

        if (node->key1 == key1 && node->key2 == key2)
            return node;

        ++it1;
    }

    node = new mctsNodeInfo();
    MCTSNodeCount.fetch_add(1, std::memory_order_relaxed);
    node->key1           = key1;
    node->key2           = key2;
    node->node_visits    = 0;
    node->number_of_sons = 0;
    node->lastMove       = Move::none();
    node->ttValue        = VALUE_NONE;
    node->AB             = false;

    MCTS.insert(std::make_pair(key1, node));

    return node;
}

void MonteCarlo::add_prior_to_node(mctsNodeInfo* node, Move m, Reward prior) const {

    LOCK(this, node);

    assert(node->number_of_sons < MAX_CHILDREN);
    assert(prior >= 0 && prior <= 1.0);

    int n = node->number_of_sons;
    if (n < MAX_CHILDREN)
    {
        node->children[n]->visits          = 0;
        node->children[n]->move            = m;
        node->children[n]->prior           = prior;
        node->children[n]->actionValue     = 0.0;
        node->children[n]->meanActionValue = 0.0;
        node->number_of_sons++;
    }
    else
    {
        assert(false);
    }
}

void MonteCarlo::search(ThreadPool&        threads,
                        Search::LimitsType limits,
                        bool               isMainThread,
                        Search::Worker*    worker) {

    mctsNodeInfo* node = nullptr;
    AB_Rollout         = false;
    Reward reward      = value_to_reward(VALUE_DRAW);
    playoutsCount      = 0;
    timeExpired        = false;
    guardTriggered     = false;

    constexpr TimePoint zeroPlayoutTimeoutMs = TimePoint(1000);

    while (!should_abort(threads) && !noLegalMoves)
    {
        if (playoutsCount == 0 && elapsed_ms() >= zeroPlayoutTimeoutMs)
        {
            guardTriggered = true;
            break;
        }

        node = tree_policy(threads, limits);
        if (!node || should_abort(threads))
            break;

        LOCK(this, node);

        if (AB_Rollout)
        {
            if (should_abort(threads))
                break;
            Value value = evaluate_with_minimax(node, std::min(ply, MAX_PLY - ply - 2));
            if (should_abort(threads))
                break;

            if (value == VALUE_ZERO)
                value = node->ttValue;

            if (value >= VALUE_KNOWN_WIN)
                value = VALUE_KNOWN_WIN - ply;

            if (value <= -VALUE_KNOWN_WIN)
                value = -(VALUE_KNOWN_WIN - ply);

            reward        = value_to_reward(value);
            node->ttValue = value;

            if (ply > maximumPly)
                maximumPly = ply;
        }
        else
        {
            reward = playout_policy(node, threads);
        }

        if (ply >= 1)
            node->ttValue = backup(reward, AB_Rollout, threads);

        ++playoutsCount;

        if (should_emit_pv(isMainThread))
            emit_pv(worker, threads);

        if (should_emit_info(isMainThread))
            emit_info(threads);

        if (should_abort(threads))
            break;

        if (elapsed_ms() > TimePoint(60000) || playoutsCount > 100000000ULL)
        {
            guardTriggered = true;
            break;
        }
    }

    if (ply >= 1)
        backup(reward, AB_Rollout, threads);

    if (should_emit_pv(isMainThread))
        emit_pv(worker, threads);

    if (should_emit_info(isMainThread))
        emit_info(threads);

    if (isMainThread && timeExpired)
        threads.stop = true;
}

MonteCarlo::MonteCarlo(Position& p, Search::Worker* worker, TranspositionTable& transpositionTable) :
    pos(p),
    thisThread(worker),
    tt(transpositionTable) {
    default_parameters();
    create_root(worker);
}

void MonteCarlo::create_root(Search::Worker* worker) {

    assert(ply == 0);
    assert(nodes[1] == nullptr);
    assert(root == nullptr);

    ply            = 1;
    maximumPly     = 1;
    startTime      = now();
    lastOutputTime = startTime;
    lastInfoTime   = startTime;
    emittedSearchMarker = false;

    for (auto& currentStack : stackBuffer)
    {
        currentStack = Search::Stack();
    }

    for (int i = -7; i <= MAX_PLY + 10; i++)
    {
        stack[i].continuationHistory =
          &worker->continuationHistory[0][0][NO_PIECE][0];
        stack[i].continuationCorrectionHistory =
          &worker->continuationCorrectionHistory[NO_PIECE][0];
        stack[i].staticEval = VALUE_NONE;
        stack[i].reduction  = 0;
    }
    for (int i = 0; i <= MAX_PLY + 2; ++i)
    {
        stack[i].ply       = i;
        stack[i].reduction = 0;
    }

    std::memset(nodesBuffer, 0, sizeof(nodesBuffer));

    root = nodes[ply] = get_node(this, pos);

    LOCK(this, root);

    root->node_visits    = 0;
    root->number_of_sons = 0;
    generate_root_moves(root);

    noLegalMoves = root->number_of_sons == 0;
}

void MonteCarlo::set_time_budget(TimePoint allocatedTime, bool useTime) {
    useTimeBudget = useTime;
    endTime       = useTimeBudget ? startTime + allocatedTime : TimePoint(0);
    hardStopTime  = useTimeBudget ? endTime + TimePoint(200) : TimePoint(0);
}

bool MonteCarlo::computational_budget(ThreadPool& threads, Search::LimitsType limits) {

    (void) limits;

    if (noLegalMoves)
        return false;

    if (threads.stop.load(std::memory_order_relaxed) || stop_requested())
        return false;

    if (useTimeBudget && now() >= endTime)
    {
        timeExpired = true;
        return false;
    }

    if (useTimeBudget && hardStopTime && now() >= hardStopTime)
    {
        timeExpired = true;
        return false;
    }

    return true;
}

bool MonteCarlo::should_abort(ThreadPool& threads) {
    if (threads.stop.load(std::memory_order_relaxed) || stop_requested())
        return true;

    if (useTimeBudget && now() >= endTime)
    {
        timeExpired = true;
        return true;
    }

    if (useTimeBudget && hardStopTime && now() >= hardStopTime)
    {
        timeExpired = true;
        return true;
    }

    return false;
}

mctsNodeInfo* MonteCarlo::tree_policy(ThreadPool& threads, Search::LimitsType limits) {

    assert(ply == 1);

    if (should_abort(threads))
        return nullptr;

    if (root->number_of_sons == 0)
    {
        return root;
    }

    mctsNodeInfo* node = nullptr;
    while ((node = nodes[ply]))
    {
        if (should_abort(threads))
            return nullptr;

        LOCK(this, node);

        if (node->node_visits == 0)
            break;

        if (!computational_budget(threads, limits) || is_terminal(node))
            return nullptr;

        edges[ply] = best_child(node, STAT_UCB);

        const Move m = edges[ply]->move;

        Edge* edge = edges[ply];

        node->node_visits++;

        edge->visits          = edge->visits + 1.0;
        edge->meanActionValue = edge->actionValue / edge->visits;

        assert(m.is_ok());
        assert(pos.legal(m));

        do_move(m);

        if (should_abort(threads))
            return nullptr;

        nodes[ply] = get_node(this, pos);
        if (nodes[ply] == nullptr)
        {
            break;
        }
    }

    if (node)
    {
        LOCK(this, node);

        const size_t greedy = TRand<size_t>(0, 100);
        if (!is_root(node) && node->ttValue < VALUE_KNOWN_WIN && node->ttValue > -VALUE_KNOWN_WIN
            && (node->number_of_sons > 5 && greedy >= mctsMultiStrategy))
        {
            AB_Rollout = true;
        }
    }

    return node;
}

Reward MonteCarlo::playout_policy(mctsNodeInfo* node, ThreadPool& threads) {

    if (should_abort(threads))
        return REWARD_DRAW;

    LOCK(this, node);

    if (is_terminal(node))
        return evaluate_terminal(node);

    if (node->node_visits == 0)
    {
        generate_moves(node, threads);
        assert(node->node_visits == 1);
    }

    if (should_abort(threads))
        return REWARD_DRAW;

    if (node->number_of_sons == 0)
        return evaluate_terminal(node);

    return node->children[0]->prior;
}

Value MonteCarlo::backup(Reward r, bool AB_Mode, ThreadPool& threads) {

    assert(ply >= 1);
    double weight = 1.0;

    while (ply != 1)
    {
        if (should_abort(threads))
            break;

        undo_move();

        r = 1.0 - r;

        Edge* edge = edges[ply];

        if (AB_Mode)
        {
            edge->prior = r;
            AB_Mode     = false;
        }

        edge->visits = edge->visits - 1.0;

        edge->visits          = edge->visits + weight;
        edge->actionValue     = edge->actionValue + weight * r;
        edge->meanActionValue = edge->actionValue / edge->visits;

        assert(edge->meanActionValue >= 0.0);
        assert(edge->meanActionValue <= 1.0);

        const double minimax = best_child(nodes[ply], STAT_MEAN)->meanActionValue;

        r = r * (1.0 - BACKUP_MINIMAX) + minimax * BACKUP_MINIMAX;

        assert(stack[ply].currentMove == edge->move);
    }

    assert(ply == 1);

    return reward_to_value(r);
}

Edge* MonteCarlo::best_child(mctsNodeInfo* node, EdgeStatistic statistic) const {

    LOCK(this, node);

    if (node->number_of_sons <= 0)
        return &EDGE_NONE;

    int    best      = -1;
    double bestValue = -1000000000000.0;
    for (int k = 0; k < node->number_of_sons; k++)
    {
        const double r =
          statistic == STAT_VISITS ? node->children[k]->visits.load(std::memory_order_relaxed)
          : statistic == STAT_MEAN
            ? node->children[k]->meanActionValue.load(std::memory_order_relaxed)
          : statistic == STAT_UCB
            ? ucb(node->children[k], node->node_visits.load(std::memory_order_relaxed), false)
          : statistic == STAT_PRIOR
            ? ucb(node->children[k], node->node_visits.load(std::memory_order_relaxed), true)
            : 0.0;

        if (r > bestValue)
        {
            bestValue = r;
            best      = k;
        }
    }

    return node->children[best];
}

bool MonteCarlo::should_emit_pv(bool isMainThread) const {

    if (!isMainThread)
        return false;

    if (ply != 1)
        return false;

    const TimePoint elapsed     = now() - startTime + 1;
    const TimePoint outputDelay = now() - lastOutputTime;

    if (elapsed < 1100)
        return outputDelay >= 100;
    else if (elapsed < static_cast<int64_t>(11 * 1000))
        return outputDelay >= 1000;
    else if (elapsed < static_cast<int64_t>(61 * 1000))
        return outputDelay >= 10000;
    else if (elapsed < static_cast<int64_t>(6 * 60 * 1000))
        return outputDelay >= 30000;
    else if (elapsed < static_cast<int64_t>(61 * 60 * 1000))
        return outputDelay >= 60000;

    return outputDelay >= 60000;
}

bool MonteCarlo::should_emit_info(bool isMainThread) const {

    if (!isMainThread)
        return false;

    if (ply != 1)
        return false;

    const TimePoint outputDelay = now() - lastInfoTime;

    return outputDelay >= 200;
}

void MonteCarlo::emit_pv(Search::Worker* worker, ThreadPool& threads) {

    assert(ply == 1);

    LOCK(this, root);

    int n = root->number_of_sons;

    EdgeArray list(root->children);

    if (mctsThreads > 1)
        std::sort(list.begin(), list.begin() + n, ComparePrior);
    else
        std::sort(list.begin(), list.begin() + n, CompareRobustChoice);

    Search::RootMoves& rootMoves = thisThread->root_moves();
    rootMoves.clear();

    if (n > 0)
    {
        for (int k = 0; k < n; k++)
        {
            rootMoves.push_back(Search::RootMove(list[k]->move));
            const size_t index             = rootMoves.size() - 1;
            rootMoves[index].previousScore = reward_to_value(list[k]->meanActionValue);
            rootMoves[index].score         = rootMoves[index].previousScore;
            rootMoves[index].selDepth      = maximumPly;
            if (k > 0)
            {
                rootMoves[index].score = rootMoves[index].previousScore - k * 10;
            }
        }

        Move move = Move::none();
        if (!rootMoves.empty() && !rootMoves[0].pv.empty())
            move = rootMoves[0].pv[0];
        int  cnt  = 0;
        while (move != Move::none() && pos.legal(move))
        {
            cnt++;
            do_move(move);
            mctsNodeInfo* node = nodes[ply] = get_node(this, pos);
            if (node == nullptr)
            {
                break;
            }
            LOCK(this, node);

            if (ply > maximumPly)
                maximumPly = ply;

            if (is_terminal(node) || node->number_of_sons <= 0 || node->node_visits <= 0)
                break;

            move = best_child(node, STAT_VISITS)->move;

            if (pos.legal(move))
                rootMoves[0].pv.push_back(move);
        }

        for (int k = 0; k < cnt; k++)
            undo_move();

        assert(int(rootMoves.size()) == root->number_of_sons);
        assert(ply == 1);

        threads.main_manager()->pv(*worker, threads, tt, worker->completed_depth());
    }
    else
    {
        rootMoves.emplace_back(Move::none());
        threads.main_manager()->updates.onUpdateNoMoves(
          {0, {pos.checkers() ? -VALUE_MATE : VALUE_DRAW, pos}});
    }

    lastOutputTime = now();
}

void MonteCarlo::emit_info(ThreadPool& threads) {

    (void) threads;

    assert(ply == 1);

    LOCK(this, root);

    if (root->number_of_sons <= 0)
    {
        lastInfoTime = now();
        return;
    }

    const bool chess960 = pos.is_chess960();
    std::ostringstream pvStream;
    int movesMade = 0;

    Move move = best_child(root, STAT_VISITS)->move;
    while (move != Move::none() && pos.legal(move))
    {
        if (movesMade > 0)
            pvStream << ' ';
        pvStream << UCIEngine::move(move, chess960);

        do_move(move);
        ++movesMade;

        mctsNodeInfo* node = nodes[ply] = get_node(this, pos);
        if (node == nullptr)
            break;

        LOCK(this, node);

        if (is_terminal(node) || node->number_of_sons <= 0 || node->node_visits <= 0)
            break;

        move = best_child(node, STAT_VISITS)->move;
    }

    for (int k = 0; k < movesMade; k++)
        undo_move();

    if (pvStream.str().empty() && move != Move::none() && pos.legal(move))
        pvStream << UCIEngine::move(move, chess960);

    const TimePoint elapsed      = std::max<TimePoint>(1, elapsed_ms());
    const uint64_t  nodesVisited = playoutsCount;
    const uint64_t  nps          = nodesVisited * 1000 / elapsed;
    const int       depth        = std::max(1, maximumPly);
    const int       selDepth     = std::max(1, maximumPly);
    const Value     scoreValue =
      reward_to_value(best_child(root, STAT_MEAN)->meanActionValue.load(std::memory_order_relaxed));

    std::string pvLine = pvStream.str();

    if (!emittedSearchMarker)
    {
        emittedSearchMarker = true;
        if (pvLine.empty())
            pvLine = "BrainLearnMCTS";
        else
            pvLine = "BrainLearnMCTS " + pvLine;
    }

    sync_cout << "info depth " << depth << " seldepth " << selDepth << " score "
              << UCIEngine::format_score({scoreValue, pos}) << " nodes " << nodesVisited << " nps "
              << nps << " hashfull " << tt.hashfull() << " time " << elapsed << " pv "
              << (pvLine.empty() ? "(none)" : pvLine) << sync_endl;

    lastInfoTime = now();
}

inline bool MonteCarlo::is_root(const mctsNodeInfo* node) const {
    if (node != root)
    {
        assert(ply != 1);
        assert(nodes[ply] != root);

        return false;
    }
    else
    {
        assert(ply == 1);
        assert(nodes[ply] == root);

        return true;
    }
}

inline bool MonteCarlo::is_terminal(mctsNodeInfo* node) const {

    {
        LOCK(this, node);

        if (node->node_visits > 0 && node->number_of_sons == 0)
            return true;
    }

    if (ply >= MAX_PLY - 2)
        return true;

    if (pos.is_draw(ply - 1))
        return true;

    return false;
}

void MonteCarlo::do_move(const Move m) {

    assert(ply < MAX_PLY);
    stack[ply].ply         = ply;
    stack[ply].currentMove = m;
    stack[ply].inCheck     = pos.checkers();
    const bool capture     = pos.capture(m);

    stack[ply].continuationHistory =
      &thisThread->continuationHistory[stack[ply].inCheck][capture][pos.moved_piece(m)][m.to_sq()];
    stack[ply].continuationCorrectionHistory =
      &thisThread->continuationCorrectionHistory[pos.moved_piece(m)][m.to_sq()];

    pos.do_move(m, states[ply], &tt);

    ply++;
    if (ply > maximumPly)
        maximumPly = ply;
}

void MonteCarlo::undo_move() {

    assert(ply > 1);

    ply--;
    pos.undo_move(stack[ply].currentMove);
}

void MonteCarlo::generate_moves(mctsNodeInfo* node, ThreadPool& threads) {

    LOCK(this, node);

    if (node->node_visits != 0)
        return;

    auto [ttHit, ttData, ttWriter] = tt.probe(pos.key());
    Depth depth                    = 30;

    const PieceToHistory* contHist[] = {
      stack[ply - 1].continuationHistory, stack[ply - 2].continuationHistory,
      stack[ply - 3].continuationHistory, stack[ply - 4].continuationHistory,
      stack[ply - 5].continuationHistory, stack[ply - 6].continuationHistory};
    Move ttMove = Move::none();
    if (ttHit && ttData.move && pos.pseudo_legal(ttData.move) && pos.legal(ttData.move))
    {
        ttMove = ttData.move;
    }
    MovePicker mp(pos, ttMove, depth, &thisThread->mainHistory, &thisThread->lowPlyHistory,
                  &thisThread->captureHistory, contHist, &thisThread->pawnHistory,
                  stack[ply].ply);
    Move move;
    int  moveCount = 0;

    Reward bestPrior = REWARD_MATED;
    while (((move = mp.next_move()) != Move::none()))
    {
        if (should_abort(threads))
            return;
        if (pos.legal(move))
        {
            stack[ply].moveCount = ++moveCount;
            Reward prior         = calculate_prior(move);
            if (prior > bestPrior)
            {
                node->ttValue = reward_to_value(prior);
                bestPrior     = prior;
            }

            add_prior_to_node(node, move, prior);
        }
    }

    int n = node->number_of_sons;
    if (n > 0)
    {
        EdgeArray& children = node->children;
        std::stable_sort(children.begin(), children.begin() + n, ComparePrior);
    }

    node->node_visits++;
}

void MonteCarlo::generate_root_moves(mctsNodeInfo* node) {

    LOCK(this, node);

    if (node->node_visits != 0 || node->number_of_sons != 0)
        return;

    int    moveCount = 0;
    Reward bestPrior = REWARD_DRAW;
    for (Move move : MoveList<LEGAL>(pos))
    {
        if (stop_requested())
            return;
        stack[ply].moveCount = ++moveCount;
        const Reward prior = REWARD_DRAW;
        add_prior_to_node(node, move, prior);
    }

    if (moveCount > 0)
        node->ttValue = reward_to_value(bestPrior);

    const int n = node->number_of_sons;
    if (n > 0)
    {
        EdgeArray& children = node->children;
        std::stable_sort(children.begin(), children.begin() + n, ComparePrior);
    }

    node->node_visits++;
}

Reward MonteCarlo::evaluate_terminal(mctsNodeInfo* node) const {

    assert(is_terminal(node));

    LOCK(this, node);

    if (node->number_of_sons == 0)
        return pos.checkers() ? REWARD_MATED : REWARD_DRAW;

    if (ply >= MAX_PLY - 2)
        return REWARD_DRAW;

    return REWARD_DRAW;
}

Value MonteCarlo::evaluate_with_minimax(const Depth d) const {
    stack[ply].ply          = ply;
    stack[ply].currentMove  = Move::none();
    stack[ply].excludedMove = Move::none();

    return thisThread->minimax_value(pos, &stack[ply], d);
}

Value MonteCarlo::evaluate_with_minimax(mctsNodeInfo* node, Depth d) const {

    stack[ply].ply          = ply;
    stack[ply].currentMove  = Move::none();
    stack[ply].excludedMove = Move::none();

    constexpr auto delta = static_cast<Value>(18);

    Value alpha;
    Value beta;

    {
        LOCK(this, node);

        alpha = std::max(node->ttValue - delta, -VALUE_INFINITE);
        beta  = std::min(node->ttValue + delta, VALUE_INFINITE);
    }

    return thisThread->minimax_value(pos, &stack[ply], d, alpha, beta);
}

Reward MonteCarlo::calculate_prior(const Move m) {

    const Depth depth = ply <= 2 || pos.capture(m) || pos.gives_check(m) ? PRIOR_SLOW_EVAL_DEPTH
                                                                         : PRIOR_FAST_EVAL_DEPTH;

    do_move(m);
    const Reward prior = value_to_reward(-evaluate_with_minimax(depth));
    undo_move();

    return prior;
}

Reward MonteCarlo::value_to_reward(Value v) const {
    constexpr double k = -0.00490739829861;
    const double     r = 1.0 / (1 + std::exp(k * static_cast<int>(v)));

    assert(REWARD_MATED <= r && r <= REWARD_MATE);
    return r;
}

Value MonteCarlo::reward_to_value(Reward r) const {
    if (r > 0.99)
        return VALUE_KNOWN_WIN;
    if (r < 0.01)
        return -VALUE_KNOWN_WIN;

    constexpr double g = 203.77396313709564;
    const double     v = g * std::log(r / (1.0 - r));
    return static_cast<Value>(static_cast<int>(v));
}

void MonteCarlo::set_exploration_constant(double c) { UCB_EXPLORATION_CONSTANT = c; }

double MonteCarlo::exploration_constant() const { return UCB_EXPLORATION_CONSTANT; }

double MonteCarlo::ucb(const Edge* edge, long fatherVisits, bool priorMode) const {

    if (priorMode)
        return edge->prior;

    assert(fatherVisits > 0);

    double result = 0.0;
    if (((mctsThreads > 1) && (edge->visits > mctsMultiMinVisits))
        || ((mctsThreads == 1) && edge->visits))
    {
        result += edge->meanActionValue;
    }
    else
    {
        result += UCB_UNEXPANDED_NODE;
    }

    const double C =
      UCB_USE_FATHER_VISITS ? exploration_constant() * std::sqrt(fatherVisits) : exploration_constant();
    const double losses = edge->visits - edge->actionValue;
    const double visits = edge->visits;

    const double divisor = losses * UCB_LOSSES_AVOIDANCE + visits * (1.0 - UCB_LOSSES_AVOIDANCE);
    result += C * edge->prior / (1 + divisor);

    result += UCB_LOG_TERM_FACTOR * std::sqrt(std::log(fatherVisits) / (1 + visits));

    return result;
}

void MonteCarlo::default_parameters() {

    BACKUP_MINIMAX           = 1.0;
    PRIOR_FAST_EVAL_DEPTH    = 2;
    PRIOR_SLOW_EVAL_DEPTH    = 3;
    UCB_UNEXPANDED_NODE      = 1.0;
    UCB_EXPLORATION_CONSTANT = 1.0;
    UCB_LOSSES_AVOIDANCE     = 1.0;
    UCB_LOG_TERM_FACTOR      = 0.0;
    UCB_USE_FATHER_VISITS    = true;
}

void MonteCarlo::print_children() {

    LOCK(this, root);
    EdgeArray& children = root->children;

    if (const int n = root->number_of_sons; n > 0)
        std::sort(children.begin(), children.begin() + n, CompareRobustChoice);

    for (int k = root->number_of_sons - 1; k >= 0; k--)
    {
        std::cout << "info string move " << k + 1 << " "
                  << UCIEngine::move(children[k]->move, pos.is_chess960())
                  << std::setprecision(2) << " win% " << children[k]->prior * 100
                  << std::fixed << std::setprecision(0) << " visits " << children[k]->visits
                  << std::endl;
    }

    lastOutputTime = now();
}

void clear() { MCTS.clear(); }

}  // namespace Stockfish::BrainLearnMCTS
