#include "experience.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <iostream>

#include "misc.h"

namespace Stockfish {

Experience experience;

namespace {

constexpr char         SugarExpMagic[]   = "SugaR Experience version 2";
constexpr std::uint8_t SugarExpBlock[16] = {0x02, 0x00, 0x80, 0xE2, 0x63, 0xA4, 0x80, 0x33,
                                            0x10, 0x06, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00};

constexpr std::size_t SugarExpHeaderSize = (sizeof(SugarExpMagic) - 1) + sizeof(SugarExpBlock);

struct ExperienceEntry {
    Move move;
    int  score;
    int  depth;
    int  count;
};

}  // namespace

void Experience::clear() { table.fill({}); }

void Experience::load(const std::filesystem::path& file, bool readonly) {
    readOnly = readonly;
    std::vector<char> buffer(1 << 20);
    std::ifstream     in(file, std::ios::binary | std::ios::ate);
    if (in)
        in.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
    std::size_t size = in ? static_cast<std::size_t>(in.tellg()) : 0;

    if (!in || size < SugarExpHeaderSize + sizeof(ExperienceSlot) * TableSize)
    {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        if (readonly)
            return;

        std::vector<char> outBuffer(1 << 20);
        std::fstream      out(file, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!out)
        {
            sync_cout << "info string Experience: invalid or too small" << sync_endl;
            return;
        }
        out.rdbuf()->pubsetbuf(outBuffer.data(), outBuffer.size());
        out.write(SugarExpMagic, sizeof(SugarExpMagic) - 1);
        out.write(reinterpret_cast<const char*>(SugarExpBlock), sizeof(SugarExpBlock));
        ExperienceSlot zero{};
        for (std::size_t i = 0; i < TableSize; ++i)
            out.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
        out.flush();
        out.close();

        in.open(file, std::ios::binary | std::ios::ate);
        if (!in)
        {
            sync_cout << "info string Experience: invalid or too small" << sync_endl;
            return;
        }
        in.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
    }

    in.seekg(0);
    table.fill({});

    char magic[sizeof(SugarExpMagic) - 1];
    if (!in.read(magic, sizeof(magic)) || std::memcmp(magic, SugarExpMagic, sizeof(magic)) != 0)
    {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        return;
    }

    std::uint8_t version;
    if (!in.read(reinterpret_cast<char*>(&version), 1) || version != 2)
    {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        return;
    }
    in.ignore(sizeof(SugarExpBlock) - 1);

    in.read(reinterpret_cast<char*>(table.data()), table.size() * sizeof(ExperienceSlot));
    sync_cout << "info string Experience: loaded file " << file.string()
              << (readOnly ? " (readonly)" : "") << sync_endl;
}

void Experience::save(const std::filesystem::path& file) const {
    if (readOnly)
        return;

    std::vector<char> buffer(1 << 20);
    std::fstream      out(file, std::ios::binary | std::ios::in | std::ios::out);
    if (!out)
        return;
    out.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
    out.seekp(0);
    out.write(SugarExpMagic, sizeof(SugarExpMagic) - 1);
    out.write(reinterpret_cast<const char*>(SugarExpBlock), sizeof(SugarExpBlock));
    out.write(reinterpret_cast<const char*>(table.data()), table.size() * sizeof(ExperienceSlot));
    out.flush();
}

Move Experience::probe(
  const Position& pos, [[maybe_unused]] int width, int evalImportance, int minDepth, int maxMoves) {
    Key                          key = pos.key();
    std::vector<ExperienceEntry> vec;

    std::size_t idx = static_cast<std::size_t>(key) & (TableSize - 1);
    for (std::size_t n = 0; n < TableSize; ++n)
    {
        const ExperienceSlot& rec = table[idx];
        if (rec.move == 0)
            break;
        if (rec.key == key)
        {
            int ct = rec.count <= 0 ? 1 : rec.count;
            vec.push_back({Move(static_cast<int>(rec.move)), rec.score, rec.depth, ct});
        }
        idx = (idx + 1) & (TableSize - 1);
    }

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

void Experience::update(const Position& pos, Move move, int score, int depth) {
    if (readOnly)
        return;

    Key           key = pos.key();
    std::uint16_t mv  = static_cast<std::uint16_t>(move.raw());
    std::size_t   idx = static_cast<std::size_t>(key) & (TableSize - 1);

    for (std::size_t n = 0; n < TableSize; ++n)
    {
        ExperienceSlot& rec = table[idx];
        if (rec.move == 0)
        {
            rec.key   = key;
            rec.move  = mv;
            rec.score = static_cast<std::int16_t>(score);
            rec.depth = static_cast<std::int16_t>(depth);
            rec.count = 1;
            return;
        }
        if (rec.key == key && rec.move == mv)
        {
            int oldDepth = rec.depth;
            int newDepth = depth;
            int total    = oldDepth + newDepth;
            if (total)
                rec.score = static_cast<std::int16_t>((rec.score * oldDepth + score * newDepth) / total);
            rec.depth = static_cast<std::int16_t>(std::max(oldDepth, newDepth));
            if (rec.count < 32767)
                rec.count++;
            return;
        }
        idx = (idx + 1) & (TableSize - 1);
    }
}

}  // namespace Stockfish
