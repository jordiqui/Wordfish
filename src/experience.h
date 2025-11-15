/*
  Deepalienist self learning public interface
*/

#ifndef EXPERIENCE_H_INCLUDED
#define EXPERIENCE_H_INCLUDED

#include <optional>
#include <string>
#include <vector>

#include "types.h"

namespace Stockfish {

class OptionsMap;
class Position;

namespace Search {
struct LimitsType;
struct RootMove;
}  // namespace Search

namespace Experience {

// Read the relevant options and ensure the module is configured accordingly.
std::optional<std::string> update_settings(const OptionsMap& options);

// Returns true when self learning is currently enabled via the options.
bool is_enabled();

// Called at the beginning of a search to optionally reorder the root moves
// using stored experience data.
void on_new_position(const Position& pos, std::vector<Search::RootMove>& rootMoves);

// Called once a search is finished. The module stores the best move together
// with the achieved score when the search satisfies the configured thresholds.
void on_search_complete(const Position&               pos,
                        const std::vector<Search::RootMove>& rootMoves,
                        Value                           bestScore,
                        Value                           evalScore,
                        Depth                           searchedDepth,
                        const Search::LimitsType&       limits);

// Called on ucinewgame/reset events to make sure the experience file is safely
// written to disk.
void new_game();

// Flush any pending changes to disk.
void flush();

// Returns a human readable status string describing the currently loaded
// experience file and its statistics. The string is suitable for printing as a
// UCI "info string" message.
std::string status_summary();

}  // namespace Experience
}  // namespace Stockfish

#endif  // #ifndef EXPERIENCE_H_INCLUDED
