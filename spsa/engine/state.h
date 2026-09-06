#pragma once
#ifdef TUNGUSKA_SPSA
#include "Engine/Board.h"
#include "Engine/Evaluation.h"
#include "Engine/Search.h"

namespace Tune {
// Caller must stop and join all search workers before updating shared values.
inline void refresh(Board& board) {
    Evaluation::initAll();
    Search::init_search();
    board.material[0] = board.material[1] = 0;
    for (int piece : board.board)
        if (piece != Board::EMPTY) board.material[piece & 1] += abs(Evaluation::PIECE_VALUES[piece]);
    // resetSearchHeuristics also resets game-history indices. A parameter
    // change must not erase already played moves or repetition information.
    const int histPly = board.histPly;
    const int ply = board.ply;
    board.resetSearchHeuristics();
    board.histPly = histPly;
    board.ply = ply;
    for (int& move : board.pvArray) move = Move::NO_MOVE;
    if (board.hashTable) board.hashTable->reset();
}
}
#endif
