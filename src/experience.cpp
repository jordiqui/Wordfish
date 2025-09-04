#include "experience.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstring>
#include <array>

#include "misc.h"

namespace Stockfish {

Experience experience;

void Experience::clear() {
    table.clear();
    clear_dirty();
}

// Mix 16-bit to 64-bit with simple avalanche
static inline uint64_t mix16to64(uint16_t x) {
    uint64_t z = x;
    z ^= z << 25;
    z *= 0x9E3779B97F4A7C15ULL;
    z ^= z >> 33;
    z *= 0xC2B2AE3D27D4EB4FULL;
    z ^= z >> 29;
    return z;
}

uint64_t Experience::compose_key(uint64_t posKey, uint16_t move16) {
    return posKey ^ mix16to64(move16);
}

void Experience::insert_entry(uint64_t key, uint16_t move, int value, int depth, int count) {
    uint64_t posKey = key ^ mix16to64(move);
    auto&    vec    = table[posKey];
    for (auto& e : vec)
        if (e.move.raw() == move)
        {
            e.score = value;
            e.depth = depth;
            e.count += count;
            return;
        }
    vec.push_back({Move(static_cast<uint16_t>(move)), value, depth, count});
}

void Experience::load(const std::string& file) {
    std::string   path = file;
    std::ifstream in;
    bool          convertBin   = false;
    bool          binaryFormat = false;
    bool          isBL         = false;
    bool          isV2         = false;

    const std::string sigV2 = "SugaR Experience version 2";

    if (path.size() >= 4)
    {
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });

        if (ext == ".bin")
        {
            convertBin = true;
            in.open(path, std::ios::binary);
            path = path.substr(0, path.size() - 4) + ".exp";
            sync_cout << "info string '.bin' experience files are deprecated; converting to '"
                      << path << "'" << sync_endl;
        }
    }

    if (!convertBin)
        in.open(path, std::ios::binary);

    std::string display = path;
    if (path != file)
        display += " (from " + file + ")";

    if (!in)
    {
        sync_cout << "info string Could not open " << display << sync_endl;
        return;
    }

    // Detect format: check for SugaR Experience v2 signature
    std::string sig(sigV2.size(), '\0');
    in.read(sig.data(), sig.size());
    if (sig == sigV2)
    {
        binaryFormat = true;
        isV2         = true;
    }
    else
    {
        // Fallback to text mode
        in.close();
        in.open(path);
    }

    table.clear();

    std::size_t totalMoves     = 0;
    std::size_t duplicateMoves = 0;

    auto add_entry = [&](uint64_t key, uint16_t move, int32_t score, int32_t depth, int32_t count) {
        totalMoves++;
        uint64_t posKey = key ^ mix16to64(move);
        auto&    vec    = table[posKey];
        bool     dup    = false;
        for (auto& e : vec)
            if (e.move.raw() == move)
            {
                dup = true;
                duplicateMoves++;
                e.score = score;
                e.depth = depth;
                e.count += count;
                break;
            }
        if (!dup)
            vec.push_back({Move(static_cast<std::uint16_t>(move)), score, depth, count});
    };

    if (binaryFormat)
    {
        if (isBL)
        {
            struct BinBL {
                uint64_t key;
                int32_t  depth;
                int32_t  value;
                uint16_t move;
                uint16_t pad;
                int32_t  perf;
            };
            BinBL e;
            while (in.read(reinterpret_cast<char*>(&e), sizeof(e)))
                add_entry(e.key, e.move, e.value, e.depth, 1);
        }
        else if (isV2)
        {
            std::array<unsigned char, 62> hdr{};
            in.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
            if (!in)
                return;

            const unsigned char* p = hdr.data();
            p += 1 + 8; // version + seed
            uint32_t bucket_size = p[0] | (p[1] << 8) | (p[2] << 16);
            p += 3;
            uint32_t entry_size = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
            (void)bucket_size; // currently unused
            if (entry_size != 34)
                return;

            // Verificar que el cuerpo está alineado a 34B
            auto here = in.tellg();
            in.seekg(0, std::ios::end);
            auto endp = in.tellg();
            in.seekg(here, std::ios::beg);
            const auto body = static_cast<std::streamoff>(endp - here);
            if (body % 34 != 0)
                return;

#pragma pack(push, 1)
            struct EntryV2 {
                uint64_t key;
                uint16_t move;
                int16_t  score;
                int16_t  depth;
                int16_t  count;
                int32_t  wins;
                int32_t  losses;
                int32_t  draws;
                int16_t  flags;
                int16_t  age;
                int16_t  pad;
            };
#pragma pack(pop)
            static_assert(sizeof(EntryV2) == 34, "EntryV2 must be 34 bytes");

            EntryV2 rec{};
            while (in.read(reinterpret_cast<char*>(&rec), sizeof(rec)))
            {
                int32_t ct = rec.count <= 0 ? 1 : rec.count;
                add_entry(rec.key, rec.move, static_cast<int32_t>(rec.score),
                          static_cast<int32_t>(rec.depth), ct);
            }
        }
    }
    else
    {
        uint64_t key;
        unsigned move;
        int      score, depth, count;

        while (in >> key >> move >> score >> depth >> count)
            add_entry(key, static_cast<uint16_t>(move), score, depth, count);
    }

    std::size_t totalPositions = table.size();
    double      frag           = totalPositions ? 100.0 * duplicateMoves / totalPositions : 0.0;

    sync_cout << "info string " << display << " -> Total moves: " << totalMoves
              << ". Total positions: " << totalPositions << ". Duplicate moves: " << duplicateMoves
              << ". Fragmentation: " << std::fixed << std::setprecision(2) << frag << "%)"
              << sync_endl;

    if (convertBin)
        save(path);
}

void Experience::save(const std::string& file) const {
    std::string path = file;

    if (path.size() >= 4)
    {
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });

        if (ext == ".bin")
        {
            path = path.substr(0, path.size() - 4) + ".exp";
            sync_cout << "info string '.bin' experience files are deprecated; saving to '" << path
                      << "'" << sync_endl;
        }
    }

    std::string buffer;
    std::size_t totalMoves = 0;

    // === Registro fijo de 34 bytes (v2) ===
#pragma pack(push, 1)
    struct EntryV2 {
        uint64_t key;
        uint16_t move;
        int16_t  score;
        int16_t  depth;
        int16_t  count;
        int32_t  wins;
        int32_t  losses;
        int32_t  draws;
        int16_t  flags;
        int16_t  age;
        int16_t  pad;
    };
#pragma pack(pop)
    static_assert(sizeof(EntryV2) == 34, "EntryV2 must be 34 bytes");

    auto append_entry34 = [&](uint64_t key, uint16_t move16, int score, int depth, int count) {
        EntryV2 out{};
        out.key   = key;
        out.move  = move16;
        out.score = static_cast<int16_t>(score);
        out.depth = static_cast<int16_t>(depth);
        out.count = static_cast<int16_t>(count <= 0 ? 1 : count);
        buffer.append(reinterpret_cast<const char*>(&out), sizeof(out));
        totalMoves++;
    };

    const std::string sig = "SugaR Experience version 2";  // 26 bytes
    buffer.append(sig);
    static const unsigned char kExpHeader62[] = {
        0x02,
        0x00,0x80,0xE2,0x63,0xA4,0x80,0x33,0x10,
        0x06,0x00,0x00,
        0x22,0x00,0x00,0x00,
        0x17,0x00,0x00,0x00, 0x01,0x00,0x00,0x00, 0x02,0x00, 0xE4,0x6C,0x3F,0x41, 0x8B,0x26,0x6D,0x09,0x00,0x00,0x19,0x00,
        0x17,0x00,0x00,0x00, 0x01,0x00,0x00,0x00, 0x02,0x00, 0xE4,0x6C,0x3F,0x41, 0x8B,0x26,0xE7,0x0B,0x00,0x00,0x14,0x00,
        0x00,0x00
    };
    buffer.append(reinterpret_cast<const char*>(kExpHeader62), sizeof(kExpHeader62));

    bool wroteAny = false;
    for (const auto& [key, vec] : table)
        for (const auto& e : vec)
        {
            append_entry34(key, static_cast<uint16_t>(e.move.raw()), e.score, e.depth, e.count);
            wroteAny = true;
        }

    if (!wroteAny)
    {
        const uint64_t key    = 0xA1B2C3D4E5F60789ULL;
        const uint16_t move16 = 0x1234;
        append_entry34(key, move16, 0, 20, 1);
        wroteAny = true;
    }

    const size_t headerLen = sig.size() + sizeof(kExpHeader62);  // 26 + 62 = 88
    if (buffer.size() < headerLen)
        return;
    const size_t bodyLen = buffer.size() - headerLen;
    if (bodyLen == 0 || (bodyLen % sizeof(EntryV2)) != 0)
        return;

    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        sync_cout << "info string Could not open " << path << " for writing" << sync_endl;
        return;
    }
    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    out.close();

    std::size_t totalPositions = table.size();
    sync_cout << "info string " << path << " <- Total moves: " << totalMoves
              << ". Total positions: " << totalPositions << sync_endl;
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
            mark_dirty();
            return;
        }
    vec.push_back({move, score, depth, 1});
    mark_dirty();
}

}  // namespace Stockfish
