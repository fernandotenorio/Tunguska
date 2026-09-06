#ifndef EVALUATION_H
#define EVALUATION_H
#include "../../../spsa/engine/parameters.h"

// Tunguska is a NNUE engine, so there is no static evaluation function.
// The pieces values below are used in Board, Search and FenParser
class Evaluation {
public:
#ifdef TUNGUSKA_SPSA
    inline static int& PAWN_VAL = Tune::PawnValue;
    inline static int& KNIGHT_VAL = Tune::KnightValue;
    inline static int& BISHOP_VAL = Tune::BishopValue;
    inline static int& ROOK_VAL = Tune::RookValue;
    inline static int& QUEEN_VAL = Tune::QueenValue;
#else
    static constexpr int PAWN_VAL = Tune::PawnValue;
    static constexpr int KNIGHT_VAL = Tune::KnightValue;
    static constexpr int BISHOP_VAL = Tune::BishopValue;
    static constexpr int ROOK_VAL = Tune::RookValue;
    static constexpr int QUEEN_VAL = Tune::QueenValue;
#endif
    static const int KING_VAL = 20000;

    static int PIECE_VALUES[14];
    static void initAll();
};

#endif
