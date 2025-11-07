#ifdef USE_LIVEBOOK
    #include "LichessMaster.h"

using namespace Stockfish::Livebook;

LichessMaster::LichessMaster() :
    LichessOpening("https://explorer.lichess.ovh/masters?") {}
#endif
