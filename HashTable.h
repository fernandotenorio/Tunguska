#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "Board.h"
#include "defs.h"
#define ISMATE (Search::INFINITE - Board::MAX_DEPTH)

enum {HFNONE, HFALPHA, HFBETA, HFEXACT};

// Lock-free hash entry using XOR verification trick.
// Two 8-byte words — each is atomically read/written on x86-64.
// On store: data = pack(move, score, depth, flags); keyXorData = zKey ^ data
// On probe: recover zKey = keyXorData ^ data; verify against board zKey
// Torn reads (half from one write, half from another) fail verification → treated as miss.
class HashEntry{
	public:
		U64 keyXorData;  // zKey ^ data
		U64 data;        // packed: move(32) | score_u16(16) | depth(8) | flags(8)

		HashEntry(): keyXorData(0), data(0){}

		static U64 packData(int move, int score, int depth, int flags){
			// score is signed, store as unsigned 16-bit
			uint16_t s = (uint16_t)(int16_t)score;
			return ((U64)(uint32_t)move << 32) | ((U64)s << 16) | ((U64)(depth & 0xFF) << 8) | (U64)(flags & 0xFF);
		}

		static int unpackMove(U64 d){ return (int)(uint32_t)(d >> 32); }
		static int unpackScore(U64 d){ return (int)(int16_t)(uint16_t)((d >> 16) & 0xFFFF); }
		static int unpackDepth(U64 d){ return (int)((d >> 8) & 0xFF); }
		static int unpackFlags(U64 d){ return (int)(d & 0xFF); }
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