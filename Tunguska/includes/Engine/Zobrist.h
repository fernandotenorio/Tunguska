#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "Board.h"
#include "defs.h"

#include <random>

class Zobrist{

	public:
		static U64 getKey(const Board& board);
		static U64 xorFromTo(U64 key, int piece, int from, int to);
		static U64 xorSquare(U64 key, int piece, int sq);
		static U64 xorEP(U64 key, int sq);
		static U64 xorCastle(U64 key, int castle);
		static U64 xorSide(U64 key);
		static void init_keys();
		
	private:
		static U64 random64();
		static std::random_device rd;
		static std::mt19937_64 gen;
		static std::uniform_int_distribution<U64> dis;
		static U64 pieceKeys[64][12];
		static U64 castleKeys[16];
		static U64 epKeys[8];
		static U64 sideBlackKey;		
};

#endif