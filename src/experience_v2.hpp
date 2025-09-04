#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace Stockfish {

#pragma pack(push,1)
struct SugarV2Header {
    char signature[32];
    unsigned char rpd4[32];
};
#pragma pack(pop)

void write_sugar_v2_header(std::FILE* f);
void seed_dummy_if_empty(const std::string& path);

} // namespace Stockfish

