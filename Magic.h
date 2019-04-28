#ifndef MAGIC_H
#define MAGIC_H

#include "defs.h"
#include "BitBoardGen.h"

class Magic{
	public:
		static U64 rookMagicMask[64];
		static U64 bishopMagicMask[64];
		static U64 rookPossibleMoves[64][4096];
		static U64 bishopPossibleMoves[64][512];
		static U64 rookPossibleMovesSize[64];
		static U64 bishopPossibleMovesSize[64];
		static void magicArraysInit();
		static U64 bishopAttacksFrom(U64 occup, int from);
		static U64 rookAttacksFrom(U64 occup, int from);
		static U64 queenAttacksFrom(U64 occup, int from);
};

U64 blockerCut(int from, U64 occu, U64* directionArray, int direction, U64 possibleMoves);
int getMagicIndex(U64 configuration, U64 magic, int size);
U64 getAsIndex(U64 bitboard, int index);
void preInit();
extern const U64 rookMagic[];
extern const U64 bishopMagic[];


#endif