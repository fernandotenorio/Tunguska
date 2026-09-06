#include "Engine/Evaluation.h"


// Used in Board, Search and FenParser
int Evaluation::PIECE_VALUES[14] = { 0, 0, PAWN_VAL, -PAWN_VAL, KNIGHT_VAL, -KNIGHT_VAL, BISHOP_VAL,
                    -BISHOP_VAL, ROOK_VAL, -ROOK_VAL, QUEEN_VAL, -QUEEN_VAL, KING_VAL, -KING_VAL };


void Evaluation::initAll() {
#ifdef TUNGUSKA_SPSA
    const int values[] = { PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL };
    for (int i = 0; i < 6; ++i) {
        PIECE_VALUES[2 + 2 * i] = values[i];
        PIECE_VALUES[3 + 2 * i] = -values[i];
    }
#endif
}
