#include "experience_format.h"
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
    idx.magic        = 0x44707223u;    // LE: 23 72 70 44
    idx.salt_or_uuid = rand64();       // cualquier valor; persistente si quieres
    idx.record_size  = 0x0011;         // ajusta si cambias ExpDummyEntry
    idx.key_size     = 0x0002;         // tamaño de tu clave primaria
    idx.reserved0    = 0;
    os.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
}
