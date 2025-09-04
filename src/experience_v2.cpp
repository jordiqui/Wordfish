#include "experience_v2.hpp"
#include "experience.h"   // For Experience::save()

#include <cstring>        // std::memcpy, std::strlen

namespace Stockfish {

void write_sugar_v2_header(std::FILE* f) {
    SugarV2Header h{};
    const char* sig = "SugaR Experience version 2";
    std::memcpy(h.signature, sig, std::strlen(sig));

    // Fill the 32-byte #rpD4 block with the engine identity for Wordfish.
    // This helper must exist in your codebase; if not, replace with your local filler.
    fill_rpd4(h.rpd4, "Wordfish 2.0 dev");

    // Write the full header (32B signature + 32B rpd4 = 64B)
    std::fwrite(&h, 1, sizeof(h), f);
}

// Instead of appending a 24-byte legacy entry, delegate to Experience::save(),
// which already serializes a valid SugaR v2 .exp (including subheader) and
// seeds a 34-byte EntryV2 "dummy" when the table is empty.
void seed_dummy_if_empty(const std::string& path) {
    Experience exp;
    exp.save(path);
}

} // namespace Stockfish
