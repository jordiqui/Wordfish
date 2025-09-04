#include "experience_format.h"
#include "experience.h"
#include <cstring>
#include <fstream>
#include <random>

static void write_header(std::ofstream& os) {
    ExpHeaderV2 h{};
    std::memset(h.signature, 0, sizeof(h.signature));
    const char* sig = "SugaR Experience version 2";
    std::memcpy(h.signature, sig, std::strlen(sig));
    os.write(reinterpret_cast<const char*>(&h), sizeof(h));
}

static uint64_t rand64() {
    std::mt19937_64 rng{std::random_device{}()};
    return rng();
}

static void write_index_root(std::ofstream& os) {
    ExpIndexRoot idx{};
    idx.magic        = 0x44707223u;  // LE: 23 72 70 44
    idx.salt_or_uuid = rand64();     // cualquier valor; persistente si quieres
    idx.record_size  = 0x0011;       // ajusta si cambias ExpDummyEntry
    idx.key_size     = 0x0002;       // tamaño de tu clave primaria
    idx.reserved0    = 0;
    os.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
}

static void write_dummy_entry(std::ofstream& os) {
    ExpDummyEntry e{};
    e.zobrist = 0x9e3779b97f4a7c15ULL;  // constante dorada / cualquier hash
    e.move    = 0x1208;                 // ejemplo: from e2->e4 si usas 6+6 bits (ajusta)
    e.score   = 0;                      // neutro
    e.depth   = 1;                      // mínima profundidad
    e.count   = 1;                      // al menos 1
    os.write(reinterpret_cast<const char*>(&e), sizeof(e));
}

namespace Stockfish {

void Experience::create_empty_file(const std::string& path) {
    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    if (!os)
        return;

    write_header(os);       // 32 bytes exactos
    write_index_root(os);   // bloque root (como Revolution)
    write_dummy_entry(os);  // siembra para que HypnoS muestre estadísticas

    os.flush();
}

}  // namespace Stockfish
