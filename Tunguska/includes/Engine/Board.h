#ifndef BOARD_H
#define BOARD_H

#include "BoardState.h"
#include "FenParser.h"
#include "defs.h"
#include "BitBoardGen.h"
#include "Move.h"

//class PVTable;
//#include "PVTable.h"

class HashTable;
#include "HashTable.h"
#include <string>

class Board{
	public:
		static const int EMPTY = 0;
		static const int WHITE = 0;
		static const int BLACK = 1;
		
		// Relations
		//PIECE | SIDE = SIDE_PIECE, ie, KING | WHITE = WHITE_KING
		//piece_side - side = piece, use when ordering move
		//piece & 1 = SIDE
		static const int PAWN = 2;
		static const int KNIGHT = 4;
		static const int BISHOP = 6;
		static const int ROOK = 8;
		static const int QUEEN = 10;
		static const int KING = 12;
		
		static const int WHITE_PAWN = 2;	
		static const int BLACK_PAWN = 3;	
		static const int WHITE_KNIGHT = 4;
		static const int BLACK_KNIGHT = 5;
		static const int WHITE_BISHOP = 6;
		static const int BLACK_BISHOP = 7;
		static const int WHITE_ROOK = 8;
		static const int BLACK_ROOK = 9;
		static const int WHITE_QUEEN = 10;
		static const int BLACK_QUEEN = 11;
		static const int WHITE_KING = 12;
		static const int BLACK_KING = 13;
		
		static const int A1 = 0;
		static const int B1 = 1;
		static const int C1 = 2;
		static const int D1 = 3;
		static const int E1 = 4;
		static const int F1 = 5;
		static const int G1 = 6;
		static const int H1 = 7;
		static const int A8 = 56;
		static const int B8 = 57;
		static const int C8 = 58;
		static const int D8 = 59;
		static const int E8 = 60;
		static const int F8 = 61;
		static const int G8 = 62;
		static const int H8 = 63;
		
		static int KS_CASTLE_ATTACK[2][2];
		static int QS_CASTLE_ATTACK[2][2];		
		static int CASTLE_SQS[2][2][4];
		static const int MAX_DEPTH = 64;
		static const int MAX_MOVES = 2048;
		
		U64 bitboards[14];
		int kingSQ[2];
		int board[64];
		BoardState state;
		int material[2];
		int fullMoves; //TODO update
		U64 zKey;
		//game hist ply
		int histPly;
		//search ply
		int ply;
		U64 zHist[MAX_MOVES];
		//PVTable *pvTable;
		HashTable *hashTable;

		int pvArray[MAX_DEPTH];
		
		int searchHistory[14][64];
		int searchKillers[2][MAX_DEPTH];
		
		static std::string RANKS[8];
		static std::string FILES[8];
		static std::string START_POS;
		
		//void setPVTable(PVTable *tb);
		void setHashTable(HashTable *tb);
		void resetSearchHeuristics();
		BoardState makeMove(int move);
		BoardState makeNullMove();
		void undoMove(int move, BoardState undo);
		void undoNullMove(BoardState undo);
		bool isRepetition();
		static int strToCode(char s);
		static std::string codeToStr(int code);
		static int squareForCoord(std::string coord);
		static std::string coordForSquare(int sq);
		std::string toFEN();
		void applyMoves(std::string movesString);
		static Board fromStartPosition();
		static Board fromFEN(std::string fen);
		void print();
		Board();
		~Board();
};

#endif