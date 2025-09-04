#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "rpd4.hpp"

namespace Stockfish {

#pragma pack(push,1)
struct SugarV2Header {
    char     signature[32];
    RPD4Block rpd4;
};
#pragma pack(pop)

void write_sugar_v2_header(std::FILE* f);
void seed_dummy_if_empty(const std::string& path);

} // namespace Stockfish

