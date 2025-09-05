#include "experience.h"

#include <algorithm>
#include <cctype>
#include <future>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <zlib.h>

#include "misc.h"
#include "uci.h"

namespace Stockfish {

Experience experience;

void Experience::wait_until_loaded() const {
    if (loader.valid())
        loader.wait();
}

bool Experience::is_ready() const {
    return !loader.valid() || loader.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void Experience::clear() {
    wait_until_loaded();
    table.clear();
}

void Experience::load(const std::string& file) {
    std::string path       = file;
    bool        convertBin = false;
    bool        compressed = false;
    std::string display;

    if (path.size() >= 4)
    {
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });

        if (ext == ".bin")
        {
            convertBin = true;
            path       = path.substr(0, path.size() - 4) + ".exp";
            sync_cout << "info string '.bin' experience files are deprecated; converting to '"
                      << path << "'" << sync_endl;
        }
        else if (ext == ".ccz")
            compressed = true;
        else if (ext == ".exp")
        {
            // Default uncompressed experience format
        }
    }

    display = path;
    if (path != file)
        display += " (from " + file + ")";

    std::string buffer;
    if (compressed)
    {
        gzFile gzin = gzopen(path.c_str(), "rb");
        if (!gzin)
        {
            sync_cout << "info string Could not open " << display << sync_endl;
            return;
        }
        char tmp[1 << 15];
        int  bytes;
        while ((bytes = gzread(gzin, tmp, sizeof(tmp))) > 0)
            buffer.append(tmp, bytes);
        gzclose(gzin);
    }
    else
    {
        std::ifstream f(convertBin ? file : path, std::ios::binary);
        if (!f)
        {
            sync_cout << "info string Could not open " << display << sync_endl;
            return;
        }
        buffer.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    std::istringstream in(buffer);

    const std::string sigV2 = "SugaR Experience version 2";
    const std::string sigV1 = "SugaR";
    std::string       header(sigV2.size(), '\0');
    in.read(header.data(), header.size());
    bool isV2 = header == sigV2;
    bool isV1 = !isV2 && header.substr(0, sigV1.size()) == sigV1;
    bool isBL = !isV1 && !isV2 && buffer.size() >= 24 && (buffer.size() % 24 == 0);
    in.clear();
    in.seekg(0, std::ios::beg);

    table.clear();
    binaryFormat     = isV1 || isV2 || isBL;
    brainLearnFormat = isBL;

    std::size_t totalMoves     = 0;
    std::size_t duplicateMoves = 0;

    auto insert_entry = [&](uint64_t key, unsigned move, int score, int depth, int count) {
        totalMoves++;
        auto& vec = table[key];
        bool  dup = false;
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
                insert_entry(e.key, e.move, e.value, e.depth, 1);
        }
        else
        {
            in.seekg(isV2 ? sigV2.size() : sigV1.size(), std::ios::beg);

            struct BinV1 {
                uint64_t key;
                uint32_t move;
                int32_t  value;
                int32_t  depth;
                uint8_t  pad[4];
            };
            struct BinV2 {
                uint64_t key;
                uint32_t move;
                int32_t  value;
                int32_t  depth;
                uint16_t count;
                uint8_t  pad[2];
            };

            if (isV2)
            {
                BinV2 e;
                while (in.read(reinterpret_cast<char*>(&e), sizeof(e)))
                    insert_entry(e.key, e.move, e.value, e.depth, e.count);
            }
            else
            {
                BinV1 e;
                while (in.read(reinterpret_cast<char*>(&e), sizeof(e)))
                    insert_entry(e.key, e.move, e.value, e.depth, 1);
            }
        }
    }
    else
    {
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string        keyStr, moveStr;
            int                score, depth, count;

            if (!(iss >> keyStr >> moveStr >> score >> depth >> count))
                continue;

            auto parse = [](const std::string& s, uint64_t& out) {
                std::istringstream ss(s);
                if (s.find_first_not_of("0123456789") == std::string::npos)
                    ss >> out;
                else
                    ss >> std::hex >> out;
                return !ss.fail();
            };

            uint64_t key64, move64;
            if (!parse(keyStr, key64) || !parse(moveStr, move64))
                continue;
            insert_entry(key64, static_cast<unsigned>(move64), score, depth, count);
        }
    }

    std::size_t totalPositions = table.size();
    double      frag           = totalPositions ? 100.0 * duplicateMoves / totalPositions : 0.0;

    sync_cout << "info string " << display << " -> Total moves: " << totalMoves
              << ". Total positions: " << totalPositions << ". Duplicate moves: " << duplicateMoves
              << ". Fragmentation: " << std::fixed << std::setprecision(2) << frag << "%)"
              << sync_endl;

    binaryFormat = true;

    if (convertBin)
        save(path);
}

void Experience::load_async(const std::string& file) {
    loader = std::async(std::launch::async, [this, file] { load(file); });
}

void Experience::save(const std::string& file) const {
    wait_until_loaded();
    std::string path       = file;
    bool        compressed = false;

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
        else if (ext == ".ccz")
            compressed = true;
        else if (ext == ".exp")
        {
            // Default uncompressed experience format
        }
    }

    std::string buffer;
    std::size_t totalMoves = 0;

    if (brainLearnFormat)
    {
        struct BinBL {
            uint64_t key;
            int32_t  depth;
            int32_t  value;
            uint16_t move;
            uint16_t pad;
            int32_t  perf;
        };
        for (const auto& [key, vec] : table)
            for (const auto& e : vec)
            {
                BinBL be{key, e.depth, e.score, static_cast<uint16_t>(e.move.raw()), 0, e.count};
                buffer.append(reinterpret_cast<const char*>(&be), sizeof(be));
                totalMoves++;
            }
    }
    else
    {
        const std::string sig = "SugaR Experience version 2";
        buffer.append(sig);
        struct BinV2 {
            uint64_t key;
            uint32_t move;
            int32_t  value;
            int32_t  depth;
            uint16_t count;
            uint8_t  pad[2];
        };
        for (const auto& [key, vec] : table)
            for (const auto& e : vec)
            {
                BinV2 be{key,
                         static_cast<uint32_t>(e.move.raw()),
                         e.score,
                         e.depth,
                         static_cast<uint16_t>(std::min(e.count, 0xFFFF)),
                         {0, 0}};
                buffer.append(reinterpret_cast<const char*>(&be), sizeof(be));
                totalMoves++;
            }
    }

    bool ok = false;
    if (compressed)
    {
        gzFile out = gzopen(path.c_str(), "wb9");
        if (out)
        {
            if (gzwrite(out, buffer.data(), buffer.size()) == (int) buffer.size())
                ok = true;
            gzclose(out);
        }
    }
    else
    {
        std::ofstream out(path, std::ios::binary);
        if (out)
        {
            out.write(buffer.data(), buffer.size());
            ok = bool(out);
        }
    }

    if (!ok)
    {
        sync_cout << "info string Could not open " << path << " for writing" << sync_endl;
        return;
    }

    std::size_t totalPositions = table.size();

    sync_cout << "info string " << path << " <- Total moves: " << totalMoves
              << ". Total positions: " << totalPositions << sync_endl;
}
Move Experience::probe(Position& pos, int width, int evalImportance, int minDepth, int maxMoves) {
    if (!is_ready())
        return Move::none();
    auto it = table.find(pos.key());
    if (it == table.end())
        return Move::none();

    auto vec = it->second;
    if (vec.empty())
        return Move::none();

    // Order moves by their historical evaluation and depth so that the most
    // promising moves come first.  This allows the engine to "learn" from
    // previous games by preferring moves with the best average score at the
    // deepest search.
    std::sort(vec.begin(), vec.end(), [&](const ExperienceEntry& a, const ExperienceEntry& b) {
        return (a.score + evalImportance * a.depth) > (b.score + evalImportance * b.depth);
    });

    vec.resize(std::min<int>({maxMoves, width, static_cast<int>(vec.size())}));

    if (vec.empty() || vec.front().depth < minDepth)
        return Move::none();

    // Pick the best move deterministically instead of randomly.  The highest
    // ranked move represents the one with the best historical evaluation.
    return vec.front().move;
}

void Experience::update(Position& pos, Move move, int score, int depth) {
    if (!is_ready())
        return;
    auto& vec = table[pos.key()];
    for (auto& e : vec)
        if (e.move == move)
        {
            // Update the stored statistics with a running average of the
            // evaluation.  This simple learning mechanism increases the score
            // reliability over time and retains the deepest search depth seen.
            e.score = (e.score * e.count + score) / (e.count + 1);
            e.depth = std::max(e.depth, depth);
            e.count++;
            return;
        }
    // First encounter of this move in the current position.
    vec.push_back({move, score, depth, 1});
}

void Experience::show(const Position& pos, int evalImportance, int maxMoves) const {
    if (!is_ready())
        return;
    auto it = table.find(pos.key());
    if (it == table.end())
    {
        sync_cout << "info string No experience available" << sync_endl;
        return;
    }
    auto vec = it->second;
    std::sort(vec.begin(), vec.end(), [&](const ExperienceEntry& a, const ExperienceEntry& b) {
        return (a.score + evalImportance * a.depth) > (b.score + evalImportance * b.depth);
    });
    int shown = 0;
    for (const auto& e : vec)
    {
        if (shown++ >= maxMoves)
            break;
        sync_cout << "info string " << UCIEngine::move(e.move, pos.is_chess960()) << " score "
                  << e.score << " depth " << e.depth << " count " << e.count << sync_endl;
    }
}

}  // namespace Stockfish
