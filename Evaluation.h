#ifndef EVALUATION_H
#define EVALUATION_H

#include "Board.h"

class Evaluation{
	
	public:
	static int evaluate(Board& board, int side);

	static const int PAWN_VAL = 100;
	static const int KNIGHT_VAL = 320;
	static const int BISHOP_VAL = 330;
	static const int ROOK_VAL = 500;
	static const int QUEEN_VAL = 1000;
	static const int KING_VAL = 20000;

	static int MIRROR64[64];
	static int PAWN_SQ[64];							
	static int KNIGHT_SQ[64];							
	static int BISHOP_SQ[64];						  
	static int ROOK_SQ[64];							 
	static int QUEEN_SQ[64];
	static int KING_SQ[64];							
	static int KING_END_SQ[64];

	//idx => Board Piece code
	static int PIECE_VALUES[14];
	static int PIECE_SQUARES[14][64];
	static int PIECE_SQUARES_END[14][64];
	static int ISOLATED_PAWN_PENALTY[8];
	static int PASSED_PAWN_BONUS[2][8];
	static int PAWN_CONNECTED_BONUS_MG[2][64];
	static int PAWN_CONNECTED_BONUS_EG[2][64];

	static bool materialDraw(const Board& board);
	static int materialValueSide(Board& board, int side);
	static int materialBalance(Board& board);
	static int pieceSquaresBalance(Board& board);
	static int pieceSquaresBalanceEnd(Board& board);
	static int kingAttacked(Board& board, int side);
	static int isolatedPawnsSide(Board& board, int side);
	static int passedPawnsSide(Board& board, int side);
	static int isolatedPawns(Board& board);
	static int passedPawns(Board& board);
	static int pieceOpenFileSide(Board& board, int side, int pieceType, int bonusOpen, int bonusSemiOpen);
	static int pieceOpenFile(Board& board);
	static int kingDistToEnemyPawnsSide(Board& board, int side);
	static int kingDistToEnemyPawns(Board& board);
	static int kingShelterSide(Board& board, int side);
	static std::pair<int, int> mobilitySide(Board& board, int side);
	static std::pair<int, int> mobility(Board& board);
	static std::pair<int, int> pawnConnectedSide(Board& board, int side);
	static std::pair<int, int> pawnConnected(Board& board);
	static std::pair<int, int> evalBishops(Board& board);

	static int get_phase(Board& board);
	static int countMaterial(Board& board, int piece);
	static Board mirrorBoard(Board& board);
	static void testEval(std::string test_file);
	static void initAll();
					
};

#endif