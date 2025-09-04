#include "experience_v2.hpp"
#include "position.h"

#include <cstring>

namespace Stockfish {

void write_sugar_v2_header(std::FILE* f) {
    SugarV2Header h{};
    const char* sig = "SugaR Experience version 2";
    std::memcpy(h.signature, sig, std::strlen(sig));
    fill_rpd4(h.rpd4, "Wordfish 2.0 dev");
    std::fwrite(&h, 1, sizeof(h), f);
}

#pragma pack(push,1)
struct ExpEntry {
    uint64_t key;
    int32_t  depth;
    int32_t  score;
    int32_t  mov;
    int32_t  perf;
};
#pragma pack(pop)

static inline int32_t pack_book_move(int from, int to, int promo) {
    return (to & 63) << 6 | (from & 63) | ((promo & 15) << 12);
}

void seed_dummy_if_empty(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "ab");
    if (!f)
        return;

    constexpr const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    StateInfo st;
    Position pos;
    pos.set(StartFEN, false, &st);
    uint64_t key = pos.key();

    ExpEntry e{};
    e.key   = key;
    e.depth = 8;
    e.score = 0;
    e.mov   = pack_book_move(12, 28, 0); // e2e4
    e.perf  = 0;

    std::fwrite(&e, 1, sizeof(e), f);
    std::fclose(f);
}

} // namespace Stockfish

