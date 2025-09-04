#include "experience.h"
#include "experience_format.h"

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

constexpr char          ExpMagic[] = "SugaR Experience version 2";
constexpr std::uint64_t ExpSeed    = 0x06103380A463E280ULL;

struct ProbeEntry {
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
    table.fill({});

    // Try compact experience format (ExpHeaderV2 + ExpIndexRoot)
    if (in && size >= sizeof(ExpHeaderV2) + sizeof(ExpIndexRoot))
    {
        in.seekg(0);
        ExpHeaderV2 h{};
        if (in.read(reinterpret_cast<char*>(&h), sizeof(h)))
        {
            bool ok_sig = (std::memcmp(h.signature, "SugaR Experience version 2", 27) == 0);
            if (ok_sig)
            {
                ExpIndexRoot idx{};
                if (in.read(reinterpret_cast<char*>(&idx), sizeof(idx)) && idx.magic == 0x44707223u)
                {
                    constexpr std::uint16_t expected_record_size = 0x0011;
                    constexpr std::uint16_t expected_key_size    = 0x0002;
                    if (idx.record_size == expected_record_size
                        && idx.key_size == expected_key_size)
                    {
                        sync_cout << "info string Experience: loaded file " << file.string()
                                  << (readOnly ? " (readonly)" : "") << sync_endl;
                        return;
                    }
                }
            }
        }
        in.clear();
        in.seekg(0);
    }

    const std::uint32_t tableBytes = TableSize * sizeof(ExpEntry);
    const std::size_t   expected   = sizeof(ExpHeader) + tableBytes;

    if (!in || size < expected)
    {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        if (readonly)
            return;

        std::vector<char> outBuffer(1 << 20);
        std::fstream      out(file, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!out)
        {
            sync_cout << "info string Experience: invalid or too small" << sync_endl;
            return;
        }
        out.rdbuf()->pubsetbuf(outBuffer.data(), outBuffer.size());
        ExpHeader header{};
        std::memcpy(header.magic, ExpMagic, sizeof(ExpMagic) - 1);
        header.version    = 2;
        header.seed       = ExpSeed;
        header.headerSize = sizeof(ExpHeader);
        header.tableBytes = tableBytes;
        std::memset(header.reserved, 1, sizeof(header.reserved));
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        ExpEntry zero{};
        for (std::size_t i = 0; i < TableSize; ++i)
            out.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
        out.flush();
        out.close();

        std::filesystem::resize_file(file, expected);

        in.open(file, std::ios::binary | std::ios::ate);
        if (!in)
        {
            sync_cout << "info string Experience: invalid or too small" << sync_endl;
            return;
        }
        in.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
        size = static_cast<std::size_t>(in.tellg());
    }

    in.seekg(0);

    ExpHeader header{};
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))
        || std::memcmp(header.magic, ExpMagic, sizeof(ExpMagic) - 1) != 0 || header.version != 2
        || header.headerSize != sizeof(ExpHeader) || header.tableBytes != tableBytes
        || size < header.headerSize + header.tableBytes)
    {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        return;
    }

    in.read(reinterpret_cast<char*>(table.data()), table.size() * sizeof(ExpEntry));
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
    const std::uint32_t tableBytes = TableSize * sizeof(ExpEntry);
    ExpHeader           header{};
    std::memcpy(header.magic, ExpMagic, sizeof(ExpMagic) - 1);
    header.version    = 2;
    header.seed       = ExpSeed;
    header.headerSize = sizeof(ExpHeader);
    header.tableBytes = tableBytes;
    std::memset(header.reserved, 1, sizeof(header.reserved));
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(table.data()), table.size() * sizeof(ExpEntry));
    out.flush();
    std::filesystem::resize_file(file, sizeof(ExpHeader) + tableBytes);
}

Move Experience::probe(
  const Position& pos, [[maybe_unused]] int width, int evalImportance, int minDepth, int maxMoves) {
    Key                     key = pos.key();
    std::vector<ProbeEntry> vec;

    std::size_t idx = static_cast<std::size_t>(key) & (TableSize - 1);
    for (std::size_t n = 0; n < TableSize; ++n)
    {
        const ExpEntry& rec = table[idx];
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

    std::sort(vec.begin(), vec.end(), [&](const ProbeEntry& a, const ProbeEntry& b) {
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
        ExpEntry& rec = table[idx];
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
                rec.score =
                  static_cast<std::int16_t>((rec.score * oldDepth + score * newDepth) / total);
            rec.depth = static_cast<std::int16_t>(std::max(oldDepth, newDepth));
            if (rec.count < 32767)
                rec.count++;
            return;
        }
        idx = (idx + 1) & (TableSize - 1);
    }
}

}  // namespace Stockfish
