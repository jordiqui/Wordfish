/*
  Deepalienist self learning compatibility helpers
  This header exposes helpers to import legacy experience book formats.
*/

#ifndef EXPERIENCE_COMPAT_H_INCLUDED
#define EXPERIENCE_COMPAT_H_INCLUDED

#include <cstdint>
#include <functional>
#include <istream>

#include "types.h"

namespace Stockfish::Experience::Compat {

using EntryCallback = std::function<void(std::uint64_t,
                                         Move,
                                         Value,
                                         Value,
                                         Depth,
                                         std::uint16_t)>;

// Attempt to load a legacy binary representation of the experience book. The
// callback is invoked for each decoded entry. The function returns true on
// success and false if the provided stream does not contain a recognised
// format.
bool load_legacy_binary(std::istream& input, const EntryCallback& callback);

}  // namespace Stockfish::Experience::Compat

#endif  // #ifndef EXPERIENCE_COMPAT_H_INCLUDED
