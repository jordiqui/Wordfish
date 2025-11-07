#include "lichess_tablebase.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "position.h"
#include "uci.h"

#ifdef USE_LIVEBOOK
    #include "livebook/BaseLivebook.h"
    #include "livebook/json/json.hpp"
#endif

namespace Stockfish {
namespace LichessTB {

namespace {

#ifdef USE_LIVEBOOK
class CurlHelper : public Livebook::BaseLivebook {
   public:
    std::vector<std::pair<std::string, Analysis>> lookup(const Position&) override {
        return {};
    }

    bool get(const std::string& url, std::string& out) {
        const CURLcode code = do_request(url);
        if (code != CURLE_OK)
            return false;

        clean_buffer_from_terminator();
        out = readBuffer;
        return true;
    }
};
#endif

}  // namespace

bool probe(const Position& pos, Value& outScore, Move& outBestMove) {
#ifdef USE_LIVEBOOK
    if (pos.is_chess960())
        return false;

    if (pos.count<ALL_PIECES>() > 7)
        return false;

    std::string fen = pos.fen();
    std::replace(fen.begin(), fen.end(), ' ', '_');

    static CurlHelper helper;
    std::string       payload;
    if (!helper.get("https://tablebase.lichess.ovh/standard?fen=" + fen, payload))
        return false;

    auto json = nlohmann::json::parse(payload, nullptr, false);
    if (json.is_discarded())
        return false;

    const auto movesIt = json.find("moves");
    if (movesIt == json.end() || !movesIt->is_array() || movesIt->empty())
        return false;

    const auto& best = (*movesIt)[0];
    if (!best.contains("uci") || !best["uci"].is_string())
        return false;

    const std::string uciMove = best["uci"].get<std::string>();
    Move              move    = UCIEngine::to_move(pos, uciMove);
    if (move == Move::none())
        return false;

    auto to_lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };

    std::string category;
    if (json.contains("category") && json["category"].is_string())
        category = json["category"].get<std::string>();
    else if (best.contains("category") && best["category"].is_string())
        category = best["category"].get<std::string>();
    else
        return false;

    category = to_lower(category);

    auto read_dtm = [](const nlohmann::json& node) -> int {
        if (!node.contains("dtm"))
            return 0;

        const auto& value = node["dtm"];
        if (value.is_number_integer())
            return value.get<int>();
        if (value.is_number_float())
            return static_cast<int>(std::round(value.get<double>()));
        return 0;
    };

    int dtm = read_dtm(json);
    if (dtm == 0)
        dtm = read_dtm(best);

    dtm = std::abs(dtm);

    const auto contains = [](const std::string& text, const std::string& needle) {
        return text.find(needle) != std::string::npos;
    };

    Value score = VALUE_NONE;

    if (contains(category, "win"))
    {
        int plies = std::max(1, dtm);
        score     = mate_in(plies);
    }
    else if (contains(category, "loss"))
    {
        int plies = std::max(1, dtm);
        score     = mated_in(plies);
    }
    else if (contains(category, "draw") || contains(category, "stalemate"))
    {
        score = VALUE_DRAW;
    }
    else
        return false;

    outScore    = score;
    outBestMove = move;
    return true;
#else
    (void) pos;
    (void) outScore;
    (void) outBestMove;
    return false;
#endif
}

}  // namespace LichessTB
}  // namespace Stockfish
