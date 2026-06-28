#ifndef EVALUATION_H
#define EVALUATION_H

// Tunguska is a NNUE engine, so there is no static evaluation function.
// The pieces values below are used in Board, Search and FenParser
class Evaluation {
public:
    static const int PAWN_VAL = 100;
    static const int KNIGHT_VAL = 429;
    static const int BISHOP_VAL = 439;
    static const int ROOK_VAL = 685;
    static const int QUEEN_VAL = 1368;
    static const int KING_VAL = 20000;

    static int PIECE_VALUES[14];
    static void initAll();
};

#endif