#ifndef BITBOARD_GEN_H
#define BITBOARD_GEN_H

#include "defs.h"

class BitBoardGen {
public:
	static const int IDX_RIGHT = 0;
	static const int IDX_UP = 1;
	static const int IDX_LEFT = 2;
	static const int IDX_DOWN = 3;
	static const int IDX_UP_RIGHT = 4;
	static const int IDX_UP_LEFT = 5;
	static const int IDX_DOWN_LEFT = 6;
	static const int IDX_DOWN_RIGHT = 7;

	static const U64 ONE = 1;
	static const U64 SIX_FOUR = 64;

	static int DIRECTIONS[8][2];
	static int KNIGHT_DIRECTIONS[8][2];

	static U64 BITBOARD_DIRECTIONS[8][64];
	static U64 BITBOARD_PAWN_ATTACKS[2][64];
	static U64 BITBOARD_KING_ATTACKS[64];
	static U64 BITBOARD_KING_REGION[2][64];
	static U64 BITBOARD_KING_AHEAD[2][64][2];
	static U64 BITBOARD_KNIGHT_ATTACKS[64];
	static U64 BITBOARD_RANKS[8];
	static U64 BITBOARD_FILES[8];
	static U64 CENTER_FILES;
	static U64 KS_CASTLE_OCCUP[2];
	static U64 QS_CASTLE_OCCUP[2];
	static U64 WRAP_FILES[2];
	static U64 RECT_LOOKUP[64][64];
	static U64 ADJACENT_FILES[8];
	static U64 FRONT_SPAN[2][64];

	static U64 SQUARES_AHEAD[2][64];
	static U64 SQUARES_BEHIND[2][64];

	static U64 FRONT_ATTACK_SPAN[2][64];
	static U64 SQUARES[64];
	static int DISTANCE_SQS[64][64];
	static int DISTANCE_MAN[64][64];
	static U64 LINES_BB[64][64];
	static U64 PAWN_CONNECTED[2][64];
	static U64 RANKS_4_5_6_7[2];

	//space masks
	static U64 SPACE_MASK[2];
	static U64 QUEENSIDE_MASK;
	static U64 KINGSIDE_MASK;

	static U64 LIGHT_DARK_SQS[2];
	static int COLOR_OF_SQ[64];

	//static U64 ROOK_RAYS[64];
	//static U64 BISHOP_RAYS[64];
	//static U64 QUEEN_RAYS[64];

	static void initSliderRays();
	static void initSpaceMasks();

	static U64 setBit(U64 bb, int idx);
	static U64 zeroBit(U64 bb, int idx);
	static U64 circular_lsh(U64 target, int shift);
	static void printBB(U64 bb);
	static void generateCastleMask();
	static void generateRanks();
	static void generateFiles();
	static void generateKnight();
	static void generateKingRegion();
	static void generateKingAhead();
	static void generateKing();
	static void generatePawns();
	static void generateDirections();
	static void generateWrapFiles();
	static void initRectLookUp();
	static void generateAdjacentFiles();
	static void generateFrontSpan();
	static void generateFrontAttackSpan();
	static void generateAheadBehind();
	static void initSquares();
	static void initDistances();
	static void initLines();
	static void initPawnConnected();
	static void initColorSquares();
	static void initAll();

	static int popCount(U64 bb) {
#ifdef _MSC_VER
		return (int)__popcnt64(bb);
#else
		return __builtin_popcountll(bb);
#endif
	}
};
static void emptyBitString(char b[8][8]);
static U64 fromBit(char* p);
static U64 bitboardFromBitString(char bitString[8][8]);

#endif