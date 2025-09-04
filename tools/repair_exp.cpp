// repair_exp.cpp
// Utilitario para reparar archivos .exp (Wordfish) al layout compatible con HypnoS/BrainLearn.
// - Corrige firma "SugaR Experience version 2" con padding a 32 bytes
// - Inserta bloque índice raíz si falta (magic 0x44707223, LE)
// - Si no hay entradas, añade 1 entrada dummy
// Salida: <input>.repaired.exp; además crea <input>.bak con el original
//
// Uso:
//   repair_exp.exe "C:\\ruta\\wordfish.exp"
//
// Compilar (Clang):
//   clang++ -O2 -std=c++17 -Wall -Wextra -o repair_exp.exe repair_exp.cpp
// Compilar (GCC MinGW64):
//   g++ -O2 -std=c++17 -Wall -Wextra -o repair_exp.exe repair_exp.cpp

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>
#include <iostream>

namespace fs = std::filesystem;

#pragma pack(push, 1)

struct ExpHeaderV2 {
    char signature[32]; // "SugaR Experience version 2\0" + padding 0x00
};

struct ExpIndexRoot {
    uint32_t magic;        // 0x44707223 (LE)
    uint64_t salt_or_uuid; // valor arbitrario
    uint16_t record_size;  // sizeof(ExpDummyEntry) por defecto
    uint16_t key_size;     // 2 por defecto (ejemplo)
    uint64_t reserved0;    // 0
};

// Entrada mínima “dummy” (ajústala si tu layout real difiere)
struct ExpDummyEntry {
    uint64_t zobrist;  // clave posición
    uint16_t move;     // movimiento codificado simple
    int16_t  score;    // eval
    uint8_t  depth;    // profundidad
    uint8_t  count;    // ocurrencias
    // Total: 8 + 2 + 2 + 1 + 1 = 14 bytes
};

#pragma pack(pop)

static_assert(sizeof(ExpHeaderV2) == 32, "Header must be 32 bytes");
static_assert(sizeof(ExpDummyEntry) == 14, "Dummy entry must be 14 bytes");
static_assert(sizeof(ExpIndexRoot) == (4 + 8 + 2 + 2 + 8), "Index root unexpected size");

// Constantes
static constexpr const char* SIG = "SugaR Experience version 2";
static constexpr uint32_t MAGIC = 0x44707223u;

// Utilidades
static uint64_t rand64() {
    std::mt19937_64 rng{std::random_device{}()};
    return rng();
}

static bool has_correct_signature(const ExpHeaderV2& h) {
    // Debe comenzar por "SugaR Experience version 2" y el resto ser 0/garbage no importante siempre que haya NUL tras la cadena.
    // Para ser estrictos: comparamos los primeros 27 bytes con el literal y verificamos que h.signature[27] sea '\0'.
    const size_t L = std::strlen(SIG); // 27
    if (std::memcmp(h.signature, SIG, L) != 0) return false;
    // No exigimos que absolutamente todos los bytes tras L sean 0, pero vamos a normalizarlos al escribir.
    return true;
}

static void write_correct_signature(ExpHeaderV2& h) {
    std::memset(h.signature, 0, sizeof(h.signature));
    std::memcpy(h.signature, SIG, std::strlen(SIG));
}

static bool read_file(const std::string& path, std::vector<uint8_t>& buf) {
    std::ifstream is(path, std::ios::binary);
    if (!is) return false;
    is.seekg(0, std::ios::end);
    std::streamoff sz = is.tellg();
    if (sz < 0) return false;
    buf.resize(static_cast<size_t>(sz));
    is.seekg(0, std::ios::beg);
    is.read(reinterpret_cast<char*>(buf.data()), buf.size());
    return is.good() || is.eof();
}

static bool write_file(const std::string& path, const std::vector<uint8_t>& buf) {
    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    if (!os) return false;
    os.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    return os.good();
}

static void backup_original(const std::string& path) {
    try {
        fs::path p(path);
        fs::path bak = p;
        bak += ".bak";
        if (!fs::exists(bak))
            fs::copy_file(p, bak, fs::copy_options::overwrite_existing);
    } catch (...) {
        // Silencioso: no detenemos la reparación por fallo de backup.
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "[uso] repair_exp.exe <ruta_al_exp>\n";
        return 1;
    }
    std::string in_path = argv[1];

    // Cargar
    std::vector<uint8_t> buf;
    if (!read_file(in_path, buf)) {
        std::cerr << "No se pudo leer el archivo: " << in_path << "\n";
        return 2;
    }
    if (buf.size() < sizeof(ExpHeaderV2)) {
        std::cerr << "Archivo demasiado pequeño. Se normalizará creando cabecera/índice/entrada dummy.\n";
    }

    // Haremos la reparación a un nuevo buffer de salida
    std::vector<uint8_t> out;

    // 1) Cabecera
    ExpHeaderV2 hdr{};
    bool had_header = buf.size() >= sizeof(ExpHeaderV2);
    if (had_header) {
        std::memcpy(&hdr, buf.data(), sizeof(hdr));
    }
    // Normalizar firma SIEMPRE (por seguridad)
    write_correct_signature(hdr);
    // Escribir cabecera normalizada
    out.resize(sizeof(ExpHeaderV2));
    std::memcpy(out.data(), &hdr, sizeof(hdr));

    // 2) Índice raíz: comprobar si existe tras la cabecera en el archivo de entrada
    bool in_has_index = false;
    ExpIndexRoot idx_in{};
    if (buf.size() >= sizeof(ExpHeaderV2) + sizeof(ExpIndexRoot)) {
        std::memcpy(&idx_in, buf.data() + sizeof(ExpHeaderV2), sizeof(ExpIndexRoot));
        if (idx_in.magic == MAGIC) {
            in_has_index = true;
        }
    }

    // Preparamos un índice raíz “correcto”
    ExpIndexRoot idx{};
    idx.magic        = MAGIC;
    idx.salt_or_uuid = in_has_index ? idx_in.salt_or_uuid : rand64();
    idx.record_size  = static_cast<uint16_t>(sizeof(ExpDummyEntry));
    idx.key_size     = 0x0002; // si tu clave real difiere, cámbialo aquí
    idx.reserved0    = 0;

    // Escribir índice raíz (siempre re-normalizamos)
    size_t out_pos = out.size();
    out.resize(out.size() + sizeof(ExpIndexRoot));
    std::memcpy(out.data() + out_pos, &idx, sizeof(idx));

    // 3) Copiar el “resto” de datos del archivo original, según corresponda
    //    - Si el archivo original YA tenía índice, copiamos desde después del índice.
    //    - Si NO lo tenía, copiamos desde después de la cabecera (para no perder datos originales).
    size_t copy_start = 0;
    if (buf.size() > 0) {
        if (in_has_index) {
            copy_start = sizeof(ExpHeaderV2) + sizeof(ExpIndexRoot);
        } else if (buf.size() > sizeof(ExpHeaderV2)) {
            copy_start = sizeof(ExpHeaderV2);
        } else {
            copy_start = buf.size(); // no hay nada que copiar
        }
        if (copy_start < buf.size()) {
            size_t len = buf.size() - copy_start;
            size_t cur = out.size();
            out.resize(cur + len);
            std::memcpy(out.data() + cur, buf.data() + copy_start, len);
        }
    }

    // 4) Detectar si ya hay al menos una entrada (heurística):
    //    Consideramos que hay entradas si el tamaño de out excede cabecera + índice en >= record_size,
    //    y si (out.size() - 32 - sizeof(ExpIndexRoot)) es múltiplo de algún tamaño >= 1.
    //    Como no conocemos el layout real del resto, usaremos una regla simple:
    const size_t base_off = sizeof(ExpHeaderV2) + sizeof(ExpIndexRoot);
    bool has_any_entry = (out.size() > base_off);

    // 5) Si NO hay entradas, añadimos una dummy
    if (!has_any_entry) {
        ExpDummyEntry e{};
        e.zobrist = 0x9e3779b97f4a7c15ULL; // constante “golden ratio” de hash
        e.move    = 0x1208;   // ejemplo (ajusta a tu codificación si procede)
        e.score   = 0;        // neutro
        e.depth   = 1;        // mínima profundidad
        e.count   = 1;        // al menos 1
        size_t cur = out.size();
        out.resize(cur + sizeof(ExpDummyEntry));
        std::memcpy(out.data() + cur, &e, sizeof(e));
        std::cout << "Se añadió una entrada dummy.\n";
    }

    // 6) Guardar: <input>.repaired.exp
    fs::path out_path = fs::path(in_path).concat(".repaired.exp");

    // Backup original (best effort)
    backup_original(in_path);

    if (!write_file(out_path.string(), out)) {
        std::cerr << "No se pudo escribir: " << out_path << "\n";
        return 3;
    }

    std::cout << "Reparación completada.\n";
    std::cout << "Escrito: " << out_path << "\n";
    std::cout << "Tamaño final: " << out.size() << " bytes\n";
    return 0;
}

