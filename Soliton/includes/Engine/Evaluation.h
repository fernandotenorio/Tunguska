#ifndef EVALUATION_H
#define EVALUATION_H

#include "Engine/Board.h"

struct AttackInfo{
	U64 rooks[2];
	U64 knights[2];
	U64 bishops[2];
	U64 queens[2];
	U64 pawns[2];

	// count number of attacks against opp's king region inside computeAttacks
	// these counts are used in king attack eval.
	int rookAttackersKing[2];
	int knightAttackersKing[2];
	int bishopAttackersKing[2];
	int queenAttackersKing[2];

	AttackInfo(){
	    reset();
	}

	void reset(){
		rooks[0] = 0; 
		rooks[1] = 0;
		knights[0] = 0;
		knights[1] = 0;
		bishops[0] = 0;
		bishops[1] = 0;
		queens[0] = 0;
		queens[1] = 0;		
		pawns[0] = 0; 
		pawns[1] = 0;

		rookAttackersKing[0] = 0;
		rookAttackersKing[1] = 0;
		knightAttackersKing[0] = 0;
		knightAttackersKing[1] = 0;
		bishopAttackersKing[0] = 0;
		bishopAttackersKing[1] = 0;
		queenAttackersKing[0] = 0;
		queenAttackersKing[1] = 0;
	}
};

struct EvalInfo{
	AttackInfo attackInfo;
	U64 occup;

	EvalInfo(){
		reset();
	}

	void reset(){
		attackInfo.reset();
		occup = 0;
	}
};

class Evaluation {
public:
    // Piece Values (Used for Material initialization)
    static const int PAWN_VAL = 100;
    static const int KNIGHT_VAL = 429;
    static const int BISHOP_VAL = 439;
    static const int ROOK_VAL = 685;
    static const int QUEEN_VAL = 1368;
    static const int KING_VAL = 20000;

    // PST Arrays [PieceType][Square]
    static int PIECE_VALUES[14]; // used in Board
    static int PIECE_SQUARES_MG[14][64];
    static int PIECE_SQUARES_END[14][64];

    // Helpers
    static int MIRROR64[64];
    static int PHASE_INC[14]; // How much each piece contributes to game phase
    static const int TOTAL_PHASE = 24;

    static void initAll();
    static void materialBalance(const Board& board, int& mg, int& eg);
    static void pieceSquares(const Board& board, int& mg, int& eg, int& gamePhase);
	static void computeAttacks(const Board& board, EvalInfo& ei);
	static void evalPawns(const Board& board, EvalInfo& ei, int& mg, int& eg);
	static void pieceOpenFile(const Board& board, int& mg, int& eg);
	static void evalKingAttack(const Board& board, int& mg, EvalInfo& ei);
	static int kingAttack(const Board& board, int side, EvalInfo& ei);
	static void outposts(const Board& board, EvalInfo& ei, int& mg, int& eg);
	static void kingShelter(const Board& board, int& mg);
	static void initEvalInfo(const Board& board, EvalInfo& ei);
	static int evaluate(const Board& board);

    // Eval mirror testing
    static Board mirrorBoard(Board& board);
    static void testEval(std::string test_file);
};

#endif