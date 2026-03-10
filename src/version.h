#pragma once

#include <string>
#include <string_view>

#ifndef ENGINE_NAME
    #define ENGINE_NAME "Wordfish-4.40-100326"
#endif

#ifndef ENGINE_BUILD_DATE
    #define ENGINE_BUILD_DATE ""
#endif

namespace Stockfish::Version {

inline constexpr std::string_view Name      = ENGINE_NAME;
inline constexpr std::string_view BuildTag = ENGINE_BUILD_DATE;

inline std::string string() {
    std::string result(Name);

    if (BuildTag.empty())
        return result;

    if (!result.empty() && result.find(BuildTag) != std::string::npos)
        return result;

    const bool ends_with_joiner = !result.empty() && (result.back() == '-' || result.back() == '_');
    const bool has_space        = result.find(' ') != std::string::npos;

    if (ends_with_joiner)
        result.append(BuildTag);
    else if (!has_space && !result.empty()) {
        result.push_back('-');
        result.append(BuildTag);
    }
    else if (!result.empty()) {
        result.push_back(' ');
        result.append(BuildTag);
    }
    else
        result = std::string(BuildTag);

    return result;
}

}  // namespace Stockfish::Version
