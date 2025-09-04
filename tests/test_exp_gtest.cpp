#include <gtest/gtest.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

#pragma pack(push,1)
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
    int16_t  pad;   // padding hasta 34
};
#pragma pack(pop)
static_assert(sizeof(EntryV2) == 34, "EntryV2 must be 34 bytes");

constexpr const char* kSig = "SugaR Experience version 2";

// Cabecera v2 completa: versión+seed+bucket+entry + 2 metabloques (22B * 2) = 61B
// Bytes idénticos a los de tu save():
static const unsigned char kHeaderExtra[] = {
    0x02,                                                              // version
    0x00, 0x80, 0xE2, 0x63, 0xA4, 0x80, 0x33, 0x10,                    // seed
    0x06, 0x00, 0x00, 0x00,                                            // bucket_size = 6
    0x22, 0x00, 0x00, 0x00,                                            // entry_size  = 34
    // Meta #1
    0x17, 0x00, 0x00, 0x00,                                            // hash_bits = 23
    0x01, 0x00, 0x00, 0x00,                                            // reserved
    0x02, 0x00,                                                        // endian_tag = 0x0002
    0xE4, 0x6C, 0x3F, 0x41,                                            // k_factor = 11.978f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    // counters = 0
    // Meta #2 (duplicado)
    0x17, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x02, 0x00,
    0xE4, 0x6C, 0x3F, 0x41,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Utilidad: escribir archivo .exp con N entradas EntryV2
std::string WriteExpFile(const std::string& path, size_t nEntries) {
    std::ofstream os(path, std::ios::binary);
    if (!os) return {};

    // firma + cabecera
    os.write(kSig, std::char_traits<char>::length(kSig));
    os.write(reinterpret_cast<const char*>(kHeaderExtra), sizeof(kHeaderExtra));

    // entradas (si las hay)
    for (size_t i = 0; i < nEntries; ++i) {
        EntryV2 e{};
        e.key   = 0xA1B2C3D4E5F60789ULL + static_cast<uint64_t>(i);
        e.move  = static_cast<uint16_t>(0x1234 + i);
        e.score = static_cast<int16_t>(10 + static_cast<int>(i));
        e.depth = static_cast<int16_t>(20);
        e.count = static_cast<int16_t>(1);
        // wins/losses/draws/flags/age/pad quedan en 0
        os.write(reinterpret_cast<const char*>(&e), sizeof(e));
    }
    os.close();
    return path;
}

// Validador: firma, cabecera mínima, múltiplo de 34, y lectura de la primera entrada (si existe)
::testing::AssertionResult ValidateExpFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return ::testing::AssertionFailure() << "No se puede abrir: " << path;

    const std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    const std::string sig(kSig);
    std::vector<char> buf(sig.size());
    in.read(buf.data(), buf.size());
    if (!in)
        return ::testing::AssertionFailure() << "Truncado antes de la firma";
    if (std::string(buf.begin(), buf.end()) != sig)
        return ::testing::AssertionFailure() << "Firma inválida";

    const std::size_t headerExtra = sizeof(kHeaderExtra); // 61
    if (size < static_cast<std::streamsize>(sig.size() + headerExtra))
        return ::testing::AssertionFailure() << "Cabecera incompleta";

    const auto afterHeader = size - static_cast<std::streamsize>(sig.size() + headerExtra);
    if (afterHeader % 34 != 0)
        return ::testing::AssertionFailure() << "Cuerpo no múltiplo de 34 bytes";

    // Si no hay entradas, es válido igualmente
    if (afterHeader == 0)
        return ::testing::AssertionSuccess();

    // Leer primera entrada
    in.seekg(static_cast<std::streamoff>(sig.size() + headerExtra), std::ios::beg);
    EntryV2 first{};
    in.read(reinterpret_cast<char*>(&first), sizeof(first));
    if (!in)
        return ::testing::AssertionFailure() << "Fallo leyendo la primera entrada";

    // Sanity checks básicos
    if (first.count <= 0)
        return ::testing::AssertionFailure() << "count inválido (<=0)";

    return ::testing::AssertionSuccess();
}

} // namespace

TEST(ExperienceFileFormat, ValidEmptyButWellFormed) {
    const std::string path = "gtest_empty_valid.exp";
    ASSERT_FALSE(WriteExpFile(path, 0).empty());
    EXPECT_TRUE(ValidateExpFile(path));
}

TEST(ExperienceFileFormat, ValidWithOneEntry) {
    const std::string path = "gtest_one_entry.exp";
    ASSERT_FALSE(WriteExpFile(path, 1).empty());
    EXPECT_TRUE(ValidateExpFile(path));
}

// Test opcional contra un archivo real producido por el motor.
// Establece EXP_PATH en tu entorno para activarlo:
//   set EXP_PATH=C:\ruta\al\archivo\Experience.exp
TEST(ExperienceFileFormat, RealFileIfProvided) {
    const char* env = std::getenv("EXP_PATH");
    if (!env) {
        GTEST_SKIP() << "EXP_PATH no establecido; se omite prueba de archivo real.";
    } else {
        EXPECT_TRUE(ValidateExpFile(env)) << "El archivo real no pasó la validación.";
    }
}
