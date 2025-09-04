#ifndef EXPERIENCE_H_INCLUDED
#define EXPERIENCE_H_INCLUDED

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "position.h"
#include "types.h"

namespace Stockfish {

#pragma pack(push, 1)

struct ExpHeader {
    char          magic[32];
    std::uint32_t version;
    std::uint64_t seed;
    std::uint32_t headerSize;
    std::uint32_t tableBytes;
    std::uint8_t  reserved[256 - 32 - 4 - 8 - 4 - 4];
};

struct ExpEntry {
    std::uint64_t key;
    std::uint16_t move;
    std::int16_t  score;
    std::int16_t  depth;
    std::int16_t  count;
    std::int32_t  wins;
    std::int32_t  losses;
    std::int32_t  draws;
    std::int16_t  flags;
    std::int16_t  age;
    std::int16_t  pad;
};

#pragma pack(pop)

static_assert(sizeof(ExpHeader) == 256, "header size");
static_assert(sizeof(ExpEntry) == 34, "ExpEntry must be 34 bytes");

class Experience {
   public:
    void clear();
    void load(const std::filesystem::path& file, bool readonly);
    void save(const std::filesystem::path& file) const;
    void create_empty_file(const std::string& path);
    Move probe(const Position&      pos,
               [[maybe_unused]] int width,
               int                  evalImportance,
               int                  minDepth,
               int                  maxMoves);
    void update(const Position& pos, Move move, int score, int depth);

   private:
    void print_stats(const std::filesystem::path& file) const;
    static constexpr std::size_t TableSize = 1ULL << 16;  // must be power of two
    static_assert((TableSize & (TableSize - 1)) == 0, "TableSize must be power of two");
    std::array<ExpEntry, TableSize> table{};
    bool                            readOnly = false;
};

extern Experience experience;

}  // namespace Stockfish

#endif  // EXPERIENCE_H_INCLUDED
