#include "experience.h"
#include "experience_format.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <iostream>
#include <unordered_set>
#include <iomanip>

#include "misc.h"

namespace Stockfish {

Experience experience;

namespace {

// Constantes del formato compacto que espera HypnoS/BrainLearn
constexpr std::uint32_t kMagic   = 0x44707223u;  // "#rpD" en little-endian
constexpr std::uint16_t kRecSize = 0x0011;       // 17 bytes / entrada
constexpr std::uint16_t kKeySize = 0x0002;       // 2 bytes de clave
constexpr std::size_t   kHdrSize = 32;           // firma padded

inline void write_le32(std::ostream& os, std::uint32_t v) {
    os.put(static_cast<char>(v & 0xFF));
    os.put(static_cast<char>((v >> 8) & 0xFF));
    os.put(static_cast<char>((v >> 16) & 0xFF));
    os.put(static_cast<char>((v >> 24) & 0xFF));
}

inline void write_le16(std::ostream& os, std::uint16_t v) {
    os.put(static_cast<char>(v & 0xFF));
    os.put(static_cast<char>((v >> 8) & 0xFF));
}

inline void write_u64(std::ostream& os, std::uint64_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

inline void write_signature32(std::ostream& os) {
    char sig[32]{};
    std::memcpy(sig, "SugaR Experience version 2", 27);
    os.write(sig, sizeof(sig));
}

inline bool is_compact_exp(std::istream& in) {
    in.clear();
    in.seekg(0, std::ios::beg);
    char sig[32]{};
    if (!in.read(sig, 32))
        return false;
    if (std::memcmp(sig, "SugaR Experience version 2", 27) != 0)
        return false;
    std::uint32_t magic = 0;
    if (!in.read(reinterpret_cast<char*>(&magic), 4))
        return false;
    return magic == kMagic;
}

// Esqueleto compacto mínimo + dummy de 17 bytes
inline void create_compact_skeleton(const std::filesystem::path& path) {
    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    if (!os)
        return;

    write_signature32(os);
    write_le32(os, kMagic);
    write_u64(os, 0);             // salt/uuid
    write_le16(os, kRecSize);
    write_le16(os, kKeySize);
    write_u64(os, 0);             // reservado
    write_u64(os, 0);             // reservado

    char dummy[0x11]{};
    os.write(dummy, sizeof(dummy));
    os.flush();
}

// Si el archivo existe pero es demasiado corto, padéalo hasta 0x40 y añade dummy
inline void normalize_minimal_compact(const std::filesystem::path& path) {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!f)
        return;
    f.seekg(0, std::ios::end);
    auto sz = static_cast<std::size_t>(f.tellg());

    if (sz < 0x40) {
        f.clear();
        f.seekp(0, std::ios::end);
        std::vector<char> pad(0x40 - sz, 0);
        f.write(pad.data(), pad.size());
        sz = 0x40;
    }
    if (sz < 0x40 + 0x11) {
        f.clear();
        f.seekp(0, std::ios::end);
        char dummy[0x11]{};
        f.write(dummy, sizeof(dummy));
    }
    f.flush();
}

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
    table.fill({});

    std::vector<char> buffer(1 << 20);
    std::ifstream     in(file, std::ios::binary | std::ios::ate);
    if (in)
        in.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
    std::size_t size = in ? static_cast<std::size_t>(in.tellg()) : 0;

    if (!in) {
        if (readonly)
            return;
        create_compact_skeleton(file);
        sync_cout << "info string Experience: created compact experience " << file.string()
                  << sync_endl;
        return;
    }

    if (is_compact_exp(in)) {
        if (size < 0x40 + 0x11)
            normalize_minimal_compact(file);
        print_stats(file);
        return;
    }

    const std::uint32_t tableBytes = TableSize * sizeof(ExpEntry);
    const std::size_t   expected   = sizeof(ExpHeader) + tableBytes;
    if (size < expected) {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        if (!readonly) {
            create_compact_skeleton(file);
            sync_cout << "info string Experience: created compact experience " << file.string()
                      << sync_endl;
        }
        return;
    }

    in.clear();
    in.seekg(0);
    ExpHeader header{};
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))
        || std::memcmp(header.magic, ExpMagic, sizeof(ExpMagic) - 1) != 0 || header.version != 2
        || header.headerSize != sizeof(ExpHeader) || header.tableBytes != tableBytes
        || size < header.headerSize + header.tableBytes) {
        sync_cout << "info string Experience: invalid or too small" << sync_endl;
        if (!readonly) {
            create_compact_skeleton(file);
            sync_cout << "info string Experience: created compact experience " << file.string()
                      << sync_endl;
        }
        return;
    }

    in.read(reinterpret_cast<char*>(table.data()), table.size() * sizeof(ExpEntry));
    print_stats(file);
}

void Experience::save(const std::filesystem::path& file) const {
    if (readOnly)
        return;

    // Si el archivo ya es compacto, no lo sobreescribas con el formato antiguo.
    if (std::ifstream fin(file, std::ios::binary); fin && is_compact_exp(fin))
        return;

    // (Si no es compacto, conservamos el guardado antiguo por compatibilidad)
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

void Experience::print_stats(const std::filesystem::path& file) const {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in)
    {
        sync_cout << "info string " << file.filename().string()
                  << " -> Total moves: 0. Total positions: 0. Duplicate moves: 0. Fragmentation: 0.00%)" << sync_endl;
        return;
    }

    std::vector<char> buffer(1 << 20);
    in.rdbuf()->pubsetbuf(buffer.data(), buffer.size());

    std::unordered_set<std::uint64_t> keys;
    std::size_t                       totalMoves      = 0;
    std::size_t                       totalPositions  = 0;

    in.seekg(0, std::ios::beg);
    if (is_compact_exp(in))
    {
        in.seekg(sizeof(ExpHeaderV2) + sizeof(ExpIndexRoot), std::ios::beg);
        ExpDummyEntry e{};
        while (in.read(reinterpret_cast<char*>(&e), sizeof(e)))
        {
            if (e.move == 0)
                continue;
            ++totalMoves;
            if (keys.insert(e.zobrist).second)
                ++totalPositions;
        }
    }
    else
    {
        in.seekg(sizeof(ExpHeader), std::ios::beg);
        ExpEntry e{};
        while (in.read(reinterpret_cast<char*>(&e), sizeof(e)))
        {
            if (e.move == 0)
                continue;
            ++totalMoves;
            if (keys.insert(e.key).second)
                ++totalPositions;
        }
    }

    std::size_t duplicates    = totalMoves >= totalPositions ? totalMoves - totalPositions : 0;
    double      fragmentation = totalMoves ? (100.0 * duplicates / totalMoves) : 0.0;

    sync_cout << "info string " << file.filename().string() << " -> Total moves: " << totalMoves
              << ". Total positions: " << totalPositions
              << ". Duplicate moves: " << duplicates
              << ". Fragmentation: " << std::fixed << std::setprecision(2) << fragmentation << "%)" << sync_endl;
}

}  // namespace Stockfish
