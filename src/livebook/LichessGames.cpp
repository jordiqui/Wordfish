#ifdef USE_LIVEBOOK
    #include "LichessGames.h"

using namespace Stockfish::Livebook;
LichessGames::LichessGames() :
    LichessOpening("https://explorer.lichess.ovh/lichess?") {}

#endif