#ifdef USE_LIVEBOOK

#    include "LichessEndgame.h"
#    include "json/json.hpp"

#    include <algorithm>
#    include <cmath>
#    include <cctype>
#    include <cstdlib>
#    include <string>

using namespace Stockfish::Livebook;

namespace {

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

int read_dtm(const nlohmann::json& node) {
    const auto dtmIt = node.find("dtm");
    if (dtmIt == node.end())
        return 0;

    if (dtmIt->is_number_integer())
        return std::abs(dtmIt->get<int>());

    if (dtmIt->is_number_float())
        return std::abs(static_cast<int>(std::lround(dtmIt->get<double>())));

    return 0;
}

}  // namespace

std::string LichessEndgame::parse_uci(const nlohmann::json& move) {
    if (!move.contains("uci") || !move["uci"].is_string())
        return "";

    return move["uci"].get<std::string>();
}

Analysis* LichessEndgame::parse_analysis(const nlohmann::json& move) {
    if (!move.contains("category") || !move["category"].is_string())
        return nullptr;

    const std::string category = to_lower_copy(move["category"].get<std::string>());

    if (category == "unknown")
        return nullptr;

    const bool isWin  = category == "win";
    const bool isLoss = category == "loss";

    if (!isWin && !isLoss)
        return new Analysis(new Wdl(0, 1, 0));

    const int mate = read_dtm(move);
    const auto mate_eval = new Mate(static_cast<int32_t>(mate));

    if (isWin)
        return new Analysis(mate_eval);

    return new Analysis(mate_eval->opponent());
}

std::string LichessEndgame::format_url(const Position& position) {
    std::string fen_encoded = position.fen();
    std::replace(fen_encoded.begin(), fen_encoded.end(), ' ', '_');
    return "https://tablebase.lichess.ovh/standard?fen=" + fen_encoded;
}

#endif  // USE_LIVEBOOK
