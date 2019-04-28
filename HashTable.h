#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "Board.h"
#include "defs.h"
#define ISMATE (Search::INFINITE - Board::MAX_DEPTH)

enum {HFNONE, HFALPHA, HFBETA, HFEXACT};

class HashEntry{
	public:
		U64 zKey;
		int move;
		int score;
		int depth;
		int flags;

		HashEntry():zKey((U64)0), move(0), score(0), depth(0), flags(0){}
};

class HashTable{
	public:
		//max size in MB
		HashTable();
		HashTable(int sizeMB);
		void initHash(int size);
		static bool probeHashEntry(Board& board, int *move, int *score, int alpha, int beta, int depth);
		static int probePvMove(Board& board);
		static void storeHashEntry(Board& board, const int move, int score, const int flags, const int depth);
		static int getPVLine(int depth, Board& board);
		static bool moveExists(Board& board, int move, int side);
		void reset();

		HashEntry *table;
		static BoardState undoList[];
		//rounded down to power of 2
		U32 numEntries;
		U32 numEntries_1;
		int newWrite;
		int overWrite;
		int hit;
		int cut;
		const int DEFAULT_SIZE = 256;
};


#endif