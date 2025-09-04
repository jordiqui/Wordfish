#include "experience_format.h"
#include "experience.h"
#include "experience_v2.hpp"
#include <cstring>
#include <random>

static uint64_t rand64() {
    std::mt19937_64 rng{std::random_device{}()};
    return rng();
}

namespace Stockfish {

void Experience::create_empty_file(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return;

    write_sugar_v2_header(f); // 64-byte header

    ExpIndexRoot idx{};
    idx.magic        = 0x44707223u;  // LE: 23 72 70 44
    idx.salt_or_uuid = rand64();     // cualquier valor; persistente si quieres
    idx.record_size  = 0x0011;       // ajusta si cambias ExpDummyEntry
    idx.key_size     = 0x0002;       // tamaño de tu clave primaria
    idx.reserved0    = 0;
    std::fwrite(&idx, 1, sizeof(idx), f);

    ExpDummyEntry e{};
    e.zobrist = 0x9e3779b97f4a7c15ULL;  // constante dorada / cualquier hash
    e.move    = 0x1208;                 // ejemplo e2e4
    e.score   = 0;
    e.depth   = 1;
    e.count   = 1;
    std::fwrite(&e, 1, sizeof(e), f);

    std::fclose(f);
}

}  // namespace Stockfish
