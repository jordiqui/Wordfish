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

// Constantes del formato compacto que espera HypnoS/BrainLearn
constexpr std::uint32_t kIndexMagic = 0x44707223u;  // "#rpD" en little-endian
constexpr std::uint16_t kRecordSize = 0x0011;       // 17 bytes / entrada
constexpr std::uint16_t kKeySize    = 0x0002;       // 2 bytes de clave

// ¿Tiene cabecera compacta válida (firma + magic de índice)?
bool is_compact_file(std::ifstream& in) {
    in.clear();
    in.seekg(0, std::ios::beg);
    char sig[32] = {};
    if (!in.read(sig, 32))
        return false;
    if (std::memcmp(sig, "SugaR Experience version 2", 27) != 0)
        return false;
    std::uint32_t magic = 0;
    if (!in.read(reinterpret_cast<char*>(&magic), sizeof(magic)))
        return false;
    return magic == kIndexMagic;
}

// Crea un esqueleto compacto mínimo: firma (32) + índice + 1 registro “dummy”
void write_compact_skeleton(const std::filesystem::path& file) {
    std::fstream out(file, std::ios::binary | std::ios::out | std::ios::trunc);

    // Firma de 32 bytes (padded)
    char sig[32] = {};
    std::memcpy(sig, "SugaR Experience version 2", 27);
    out.write(sig, sizeof(sig));

    // Bloque índice raíz mínimo
    const std::uint32_t magic = kIndexMagic;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

    // Relleno genérico / campos reservados (dejamos 8 bytes a cero como timestamp/seed)
    std::uint64_t zero64 = 0;
    out.write(reinterpret_cast<const char*>(&zero64), sizeof(zero64));

    // Tamaños de registro y clave
    out.write(reinterpret_cast<const char*>(&kRecordSize), sizeof(kRecordSize));
    out.write(reinterpret_cast<const char*>(&kKeySize), sizeof(kKeySize));

    // Más reservado a cero para no romper lectores estrictos
    out.write(reinterpret_cast<const char*>(&zero64), sizeof(zero64));
    out.write(reinterpret_cast<const char*>(&zero64), sizeof(zero64));

    // Añade una “dummy” de 17 bytes a cero (evita recuento=0)
    char dummy[0x11] = {};
    out.write(dummy, sizeof(dummy));

    out.flush();
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

        // Antes: escribir ExpHeader (antiguo) + tabla.
        // Ahora: crear esqueleto compacto compatible con HypnoS/BrainLearn.
        write_compact_skeleton(file);

        sync_cout << "info string Experience: created compact experience " << file.string()
                  << sync_endl;
        return;  // Ya está listo; no intentes leer como formato antiguo.
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

    // Si el archivo ya es compacto, no lo sobreescribas con el formato antiguo.
    {
        std::ifstream fin(file, std::ios::binary);
        if (fin && is_compact_file(fin))  // firma ok + magic 0x44707223 en offset 32
            return;
    }

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

}  // namespace Stockfish
