#pragma once

#include <string>
#include <string_view>

#ifndef ENGINE_FAMILY
#    define ENGINE_FAMILY "Wordfish"
#endif

#ifndef ENGINE_BUILD_EXTRA
#    define ENGINE_BUILD_EXTRA "based on Stockfish dev-20250913"
#endif

#ifndef ENGINE_AUTHORS
#    define ENGINE_AUTHORS \
        "Jorge Ruiz Centelles and the Stockfish developers (see AUTHORS file)"
#endif

namespace Stockfish::Version {

inline constexpr std::string_view Name      = ENGINE_FAMILY;
inline constexpr std::string_view BuildTag  = ENGINE_BUILD_EXTRA;
inline constexpr std::string_view Authors   = ENGINE_AUTHORS;

inline std::string string() {
    if (BuildTag.empty())
        return std::string(Name);

    std::string result(Name);
    result += ' ';
    result.append(BuildTag);
    return result;
}

}  // namespace Stockfish::Version

