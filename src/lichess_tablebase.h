#ifndef LICHESS_TABLEBASE_H_INCLUDED
#define LICHESS_TABLEBASE_H_INCLUDED

#include "types.h"

namespace Stockfish {

class Position;

namespace LichessTB {

bool probe(const Position& pos, Value& outScore, Move& outBestMove);

}  // namespace LichessTB

}  // namespace Stockfish

#endif  // #ifndef LICHESS_TABLEBASE_H_INCLUDED
