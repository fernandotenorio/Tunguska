#ifndef EVALUATION_H
#define EVALUATION_H

#include "Board.h"

enum ending_type {KNB_K, KRR_KQ, OTHER_ENDING}; 


struct AttackCache{
	U64 rooks[2];
	U64 knights[2];
	U64 bishops[2];
	U64 queens[2];
	U64 pawns[2];
	U64 all[2];
	U64 all2[2];

	AttackCache(){
		reset();
	}

	int countAttacksAt(U64 sq, int side){
		int attacks = 0;
		
		if (sq & rooks[side]) 
			attacks++;
		if (sq & knights[side]) 
			attacks++;
		if (sq & bishops[side]) 
			attacks++;
		if (sq & queens[side]) 
			attacks++;

		return attacks;
	}

	U64 allAttacks(int side){
		return all[side];
	}

	U64 attacks2(int side){		
		return all2[side];
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
		all[0] = 0;
		all[1] = 0;
		all2[0] = 0;
		all2[1] = 0;
	}
};


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

	static void materialBalance(const Board& board, int& mg, int& eg);
	static void pieceSquaresBalance(const Board& board, int& mg, int& eg);
	static void evalPawns(const Board& board, int& mg, int& eg, AttackCache *attCache);
	static bool materialDraw(const Board& board);
	static void pieceOpenFile(const Board& board, int& mg, int& eg);
	static void kingAttack(const Board& board, int& mg, AttackCache *attCache);
	static int kingAttackedSide(const Board& board, int side, AttackCache *attCache);
	static void kingShelter(const Board& board, int& mg);
	static void kingTropism(const Board& board, int& mg);
	static int countMaterial(const Board& board, int piece);
	static int materialValueSide(const Board& board, int side);
	static void evalBishops(const Board& board, int& mg, int& eg);
	static void evalRooks(const Board& board, int& mg, int& eg);
	static void threats(const Board& board, int& mg, int& eg, AttackCache *attCache);
	static void imbalance(const Board& board, int& mg, int& eg);
	static void outposts(const Board& board, int&mg, int&eg);
	static void mobility(const Board& board, int& mg, int& eg);

	static Board mirrorBoard(Board& board);
	static void testEval(std::string test_file);
	static int get_phase(const Board& board);
	static ending_type get_ending(const Board& board);
	static void initAll();
};

#endif