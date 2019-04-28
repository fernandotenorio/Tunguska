#ifndef EVALUATION_H
#define EVALUATION_H

#include "Board.h"

enum ending_type {KNB_K, KRR_KQ, OTHER_ENDING}; 

/*
struct AttackInfo{
	U64 rooks[2];
	U64 knights[2];
	U64 bishops[2];
	U64 queens[2];
};
*/

class Evaluation{
	
	public:
	static int evaluate(const Board& board, int side);

	static const int PAWN_VAL = 100;
	static const int KNIGHT_VAL = 429;
	static const int BISHOP_VAL = 439;
	static const int ROOK_VAL = 685;
	static const int QUEEN_VAL = 1368;
	static const int KING_VAL = 20000;
	
	static int PIECE_VALUES[14];
	static int PIECE_SQUARES_MG[14][64];
	static int PIECE_SQUARES_END[14][64];
	static int MIRROR64[64];
	static int DISTANCE_BONUS[64][64];

	static void materialBalance(const Board& board, int& mg, int& eg);
	static void pieceSquaresBalance(const Board& board, int& mg, int& eg);
	static void evalPawns(const Board& board, int& mg, int& eg);
	static bool materialDraw(const Board& board);
	static void pieceOpenFile(const Board& board, int& mg, int& eg);
	static void kingAttack(const Board& board, int& mg);
	static int kingAttackedSide(const Board& board, int side);
	static void kingShelter(const Board& board, int& mg);
	static void kingTropism(const Board& board, int& mg);
	static int countMaterial(const Board& board, int piece);
	static int materialValueSide(const Board& board, int side);
	static void evalBishops(const Board& board, int& mg, int& eg);
	static void evalRooks(const Board& board, int& mg, int& eg);
	static void mobility(const Board& board, int& mg, int& eg);
	static void outposts(const Board& board, int&mg, int&eg);
	static int evalKBN_K(const Board& board, int side);
	
	static Board mirrorBoard(Board& board);
	static void testEval(std::string test_file);
	static int get_phase(const Board& board);
	static ending_type get_ending(const Board& board);
	static void initAll();
};

#endif