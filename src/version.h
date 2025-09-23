#pragma once

#include <string>
#include <string_view>

#ifndef ENGINE_NAME
    #define ENGINE_NAME "Wordfish v.2.60 230925"
#endif

#ifndef ENGINE_BUILD_DATE
    #define ENGINE_BUILD_DATE ""
#endif

namespace Stockfish::Version {

inline constexpr std::string_view Name      = ENGINE_NAME;
inline constexpr std::string_view BuildTag = ENGINE_BUILD_DATE;

inline std::string string() {
    if (BuildTag.empty())
        return std::string(Name);

    std::string result(Name);
    result += ' ';
    result.append(BuildTag);
    return result;
}

}  // namespace Stockfish::Version

