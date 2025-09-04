#include "experience_format.h"
#include <cstring>
#include <fstream>

static void write_header(std::ofstream& os) {
    ExpHeaderV2 h{};
    std::memset(h.signature, 0, sizeof(h.signature));
    const char* sig = "SugaR Experience version 2";
    std::memcpy(h.signature, sig, std::strlen(sig));
    os.write(reinterpret_cast<const char*>(&h), sizeof(h));
}
