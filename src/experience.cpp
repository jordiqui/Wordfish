#include "experience.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace Stockfish {

Experience experience;

void Experience::clear() { table.clear(); }

namespace {

constexpr char         SugarExpMagic[]   = "SugaR Experience version 2";
constexpr std::uint8_t SugarExpBlock[16] = {0x02, 0x00, 0x80, 0xE2, 0x63, 0xA4, 0x80, 0x33,
                                            0x10, 0x06, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00};

constexpr std::size_t SugarExpHeaderSize = (sizeof(SugarExpMagic) - 1) + sizeof(SugarExpBlock);
constexpr std::size_t SugarExpTableSize  = 1ULL << 16;  // must be power of two

#pragma pack(push, 1)
struct SugarEntry {
    std::uint64_t key;
    std::uint16_t move;
    std::int16_t  score;
    std::int16_t  depth;
    std::int16_t  count;
    std::int32_t  wins;
    std::int32_t  losses;
    std::int32_t  draws;
    std::int16_t  flags;
    std::int16_t  age;
    std::int16_t  pad;
};
#pragma pack(pop)

static_assert(sizeof(SugarEntry) == 34, "SugarEntry must be 34 bytes");

}  // namespace

void Experience::load(const std::string& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    bool          create = false;
    if (!in)
        create = true;
    else if (std::size_t(in.tellg()) < SugarExpHeaderSize + sizeof(SugarEntry) * SugarExpTableSize)
        create = true;

    if (create)
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out)
            return;
        out.write(SugarExpMagic, sizeof(SugarExpMagic) - 1);
        out.write(reinterpret_cast<const char*>(SugarExpBlock), sizeof(SugarExpBlock));
        std::vector<char> zero(sizeof(SugarEntry) * SugarExpTableSize, 0);
        out.write(zero.data(), zero.size());
        out.close();
        in.open(file, std::ios::binary | std::ios::ate);
        if (!in)
            return;
    }

    in.seekg(0);
    table.clear();

    char magic[sizeof(SugarExpMagic) - 1];
    if (!in.read(magic, sizeof(magic)) || std::memcmp(magic, SugarExpMagic, sizeof(magic)) != 0)
        return;

    std::uint8_t block[sizeof(SugarExpBlock)];
    if (!in.read(reinterpret_cast<char*>(block), sizeof(block))
        || std::memcmp(block, SugarExpBlock, sizeof(block)) != 0)
        return;

    SugarEntry rec;
    for (std::size_t i = 0;
         i < SugarExpTableSize && in.read(reinterpret_cast<char*>(&rec), sizeof(rec)); ++i)
    {
        if (rec.move)
        {
            int ct = rec.count <= 0 ? 1 : rec.count;
            table[rec.key].push_back({Move(static_cast<int>(rec.move)), rec.score, rec.depth, ct});
        }
    }
}

void Experience::save(const std::string& file) const {
    std::vector<SugarEntry> tableBin(SugarExpTableSize);

    for (const auto& [key, vec] : table)
        for (const auto& e : vec)
        {
            std::size_t idx = static_cast<std::size_t>(key) & (SugarExpTableSize - 1);
            for (std::size_t n = 0; n < SugarExpTableSize; ++n)
            {
                SugarEntry& rec = tableBin[idx];
                if (rec.move == 0)
                {
                    rec.key   = key;
                    rec.move  = static_cast<std::uint16_t>(e.move.raw());
                    rec.score = static_cast<std::int16_t>(e.score);
                    rec.depth = static_cast<std::int16_t>(e.depth);
                    rec.count = static_cast<std::int16_t>(std::clamp(e.count, 0, 32767));
                    break;
                }
                idx = (idx + 1) & (SugarExpTableSize - 1);
            }
        }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
        return;
    out.write(SugarExpMagic, sizeof(SugarExpMagic) - 1);
    out.write(reinterpret_cast<const char*>(SugarExpBlock), sizeof(SugarExpBlock));
    out.write(reinterpret_cast<const char*>(tableBin.data()), tableBin.size() * sizeof(SugarEntry));
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
