#include "experience.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace Stockfish {

Experience experience;

void Experience::clear() { table.clear(); }

namespace {

constexpr char         SugarExpMagic[]   = "SugaR Experience version 2";
constexpr std::uint8_t SugarExpBlock[16] = {0x02, 0x00, 0x80, 0xE2, 0x63, 0xA4, 0x80, 0x33,
                                            0x10, 0x06, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00};

constexpr std::size_t SugarExpHeaderSize = (sizeof(SugarExpMagic) - 1) + sizeof(SugarExpBlock);

#pragma pack(push, 1)

struct SugarRecord {
    uint32_t move1, visits1, key1_lo, key1_hi;
    int32_t  score1;
    uint32_t depth1;
    uint32_t move2, visits2, key2_lo, key2_hi;
    int32_t  score2;
    uint32_t depth2;
    uint32_t extraA, extraB;
};
#pragma pack(pop)

static_assert(sizeof(SugarRecord) == 56);

}  // namespace

void Experience::load(const std::string& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in)
        return;

    auto size = in.tellg();
    if (size <= std::streampos(SugarExpHeaderSize)
        || (static_cast<std::uint64_t>(size) - SugarExpHeaderSize) % sizeof(SugarRecord) != 0)
        return;

    in.seekg(0);
    table.clear();

    char magic[sizeof(SugarExpMagic) - 1];
    if (!in.read(magic, sizeof(magic)) || std::memcmp(magic, SugarExpMagic, sizeof(magic)) != 0)
        return;

    std::uint8_t block[sizeof(SugarExpBlock)];
    if (!in.read(reinterpret_cast<char*>(block), sizeof(block))
        || std::memcmp(block, SugarExpBlock, sizeof(block)) != 0)
        return;

    SugarRecord rec;
    while (in.read(reinterpret_cast<char*>(&rec), sizeof(rec)))
    {
        if (rec.move1)
        {
            uint64_t key = (uint64_t(rec.key1_hi) << 32) | rec.key1_lo;
            table[key].emplace_back(
              ExperienceEntry{Move(static_cast<int>(rec.move1)), static_cast<int16_t>(rec.score1),
                              static_cast<int16_t>(rec.depth1),
                              static_cast<uint16_t>(
                                std::max<uint32_t>(1, std::min(rec.visits1, uint32_t(65535))))});
        }
        if (rec.move2)
        {
            uint64_t key = (uint64_t(rec.key2_hi) << 32) | rec.key2_lo;
            table[key].emplace_back(
              ExperienceEntry{Move(static_cast<int>(rec.move2)), static_cast<int16_t>(rec.score2),
                              static_cast<int16_t>(rec.depth2),
                              static_cast<uint16_t>(
                                std::max<uint32_t>(1, std::min(rec.visits2, uint32_t(65535))))});
        }
    }
}

void Experience::save(const std::string& file) const {
    std::ofstream out(file, std::ios::binary);
    if (!out)
        return;

    out.write(SugarExpMagic, sizeof(SugarExpMagic) - 1);
    out.write(reinterpret_cast<const char*>(SugarExpBlock), sizeof(SugarExpBlock));

    for (const auto& [key, vecOrig] : table)
    {
        auto vec = vecOrig;
        std::sort(vec.begin(), vec.end(), [](const ExperienceEntry& a, const ExperienceEntry& b) {
            return a.count > b.count;
        });
        for (size_t i = 0; i < vec.size(); i += 2)
        {
            SugarRecord rec{};
            const auto& e1 = vec[i];
            rec.move1      = static_cast<uint32_t>(e1.move.raw());
            rec.visits1    = static_cast<uint32_t>(e1.count);
            rec.key1_lo    = static_cast<uint32_t>(key & 0xFFFFFFFFu);
            rec.key1_hi    = static_cast<uint32_t>(key >> 32);
            rec.score1     = static_cast<int32_t>(e1.score);
            rec.depth1     = static_cast<uint32_t>(e1.depth);

            if (i + 1 < vec.size())
            {
                const auto& e2 = vec[i + 1];
                rec.move2      = static_cast<uint32_t>(e2.move.raw());
                rec.visits2    = static_cast<uint32_t>(e2.count);
                rec.key2_lo    = rec.key1_lo;
                rec.key2_hi    = rec.key1_hi;
                rec.score2     = static_cast<int32_t>(e2.score);
                rec.depth2     = static_cast<uint32_t>(e2.depth);
            }

            out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
        }
    }
}

Move Experience::probe(
  Position& pos, [[maybe_unused]] int width, int evalImportance, int minDepth, int maxMoves) {
    auto it = table.find(pos.key());
    if (it == table.end())
        return Move::none();

    auto vec = it->second;
    if (vec.empty())
        return Move::none();

    std::sort(vec.begin(), vec.end(), [&](const ExperienceEntry& a, const ExperienceEntry& b) {
        return (int(a.score) + evalImportance * int(a.depth))
             > (int(b.score) + evalImportance * int(b.depth));
    });

    vec.resize(std::min<int>(maxMoves, static_cast<int>(vec.size())));
    const auto& best = vec.front();
    if (int(best.depth) < minDepth)
        return Move::none();

    return best.move;
}

void Experience::update(Position& pos, Move move, int score, int depth) {
    auto& vec = table[pos.key()];
    for (auto& e : vec)
        if (e.move == move)
        {
            e.score = static_cast<int16_t>(score);
            e.depth = static_cast<int16_t>(depth);
            e.count = static_cast<uint16_t>(e.count + 1);
            return;
        }
    vec.push_back({move, static_cast<int16_t>(score), static_cast<int16_t>(depth), 1});
}

}  // namespace Stockfish
