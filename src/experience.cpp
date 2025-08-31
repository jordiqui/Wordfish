#include "experience.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Stockfish {

Experience experience;

void Experience::clear() { table.clear(); }

void Experience::load(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return;
    table.clear();
    while (true) {
        uint64_t key;
        unsigned move;
        int      score, depth, count;
        if (!in.read(reinterpret_cast<char*>(&key), sizeof(key)))
            break;
        in.read(reinterpret_cast<char*>(&move), sizeof(move));
        in.read(reinterpret_cast<char*>(&score), sizeof(score));
        in.read(reinterpret_cast<char*>(&depth), sizeof(depth));
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        table[key].emplace_back(ExperienceEntry{Move(static_cast<std::uint16_t>(move)), score, depth, count});
    }
}

void Experience::save(const std::string& file) const {
    std::ofstream out(file, std::ios::binary);
    if (!out)
        return;
    for (const auto& [key, vec] : table)
        for (const auto& e : vec) {
            unsigned move = e.move.raw();
            out.write(reinterpret_cast<const char*>(&key), sizeof(key));
            out.write(reinterpret_cast<const char*>(&move), sizeof(move));
            out.write(reinterpret_cast<const char*>(&e.score), sizeof(e.score));
            out.write(reinterpret_cast<const char*>(&e.depth), sizeof(e.depth));
            out.write(reinterpret_cast<const char*>(&e.count), sizeof(e.count));
        }
}

Move Experience::probe(Position& pos, [[maybe_unused]] int width,
                       int evalImportance, int minDepth, int maxMoves) {
    auto it = table.find(pos.key());
    if (it == table.end())
        return Move::none();

    auto vec = it->second;
    if (vec.empty())
        return Move::none();

    std::sort(vec.begin(), vec.end(), [&](const ExperienceEntry& a, const ExperienceEntry& b) {
        return (a.score + evalImportance * a.depth) > (b.score + evalImportance * b.depth);
    });

    vec.resize(std::min<int>(maxMoves, static_cast<int>(vec.size())));
    const auto& best = vec.front();
    if (best.depth < minDepth)
        return Move::none();

    return best.move;
}

void Experience::update(Position& pos, Move move, int score, int depth) {
    auto& vec = table[pos.key()];
    for (auto& e : vec)
        if (e.move == move)
        {
            e.score = score;
            e.depth = depth;
            e.count++;
            return;
        }
    vec.push_back({move, score, depth, 1});
}

}  // namespace Stockfish
