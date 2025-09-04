#include "experience_v2.hpp"

#include <cstdio>   // std::FILE, std::fopen, std::fclose, std::fwrite, std::fseek, std::ftell, SEEK_END
#include <cstring>  // std::memcpy, std::strlen

namespace Stockfish {

// Exact 32-byte #rpD4 block for Wordfish (10-byte engine_id + CRC32 over those 10 bytes)
static const unsigned char RPD4_BLOCK[32] = {
    // "#rpD4"
    0x23, 0x72, 0x70, 0x44, 0x34,
    // CRC32 little-endian over the 10-byte engine_id (0x2BF08754)
    0x54, 0x87, 0xF0, 0x2B,
    // fields as in Revolution (ver/flags etc.)
    0x04, 0x01, 0x00, 0x11, 0x00, 0x02,
    // 7 zero bytes
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 10-byte engine_id = first 10 bytes of MD5("Wordfish 2.0 dev")
    0x68, 0x12, 0xD4, 0xB0, 0xC3, 0x8B, 0x75, 0xD2, 0x6C, 0x9F
};

// Exact 62-byte subheader as seen in Revolution (required by HypnoS/loader)
static const unsigned char SUBHEADER62[62] = {
    0x07, 0x00, 0x13, 0x00, 0x04, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x80, 0xE2, 0x63, 0xA4,
    0x80, 0x33, 0x10, 0x06, 0x00, 0x00, 0x22, 0x00,
    0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0x02, 0x00, 0xE4, 0x6C, 0x3F, 0x41,
    0x8B, 0x26, 0x6D, 0x09, 0x00, 0x00, 0x19, 0x00,
    0x17, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00,
    0x02, 0x00, 0x02, 0x00
};

// Helper: fast file size or -1 if not accessible
static inline long file_size(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return -1;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);
    return sz;
}

void write_sugar_v2_header(std::FILE* f) {
    SugarV2Header h{};
    const char* sig = "SugaR Experience version 2";
    std::memcpy(h.signature, sig, std::strlen(sig));
    // h.rpd4 is a struct (RPD4Block), take its address
    std::memcpy(&h.rpd4, RPD4_BLOCK, sizeof(RPD4_BLOCK));
    // 64B header (signature + rpd4)
    std::fwrite(&h, 1, sizeof(h), f);
    // followed by the 62B subheader
    std::fwrite(SUBHEADER62, 1, sizeof(SUBHEADER62), f);
}

// Minimal, non-blocking seeding: if file is missing or shorter than 64+62 bytes,
// create it with just the header + subheader. Actual entries will be added later
// by the normal save path, outside of "isready"/"ucinewgame" latency-sensitive paths.
void seed_dummy_if_empty(const std::string& path) {
    const long need = 64 + 62; // header + subheader
    long sz = file_size(path);
    if (sz >= need) return; // already valid

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    write_sugar_v2_header(f);
    std::fclose(f);
}

} // namespace Stockfish
