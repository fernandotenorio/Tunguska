#ifndef MOVE_H
#define MOVE_H

#include "StringUtils.h"
#include <algorithm>
#include "defs.h"

class MoveScore{
	public:
		int score;
		int move;
		
		MoveScore() { move = 0; score = 0; };
		MoveScore(int mv, int sc){
			move = mv;
			score = sc;
		}
		
		bool operator < (const MoveScore& other) const{
			return other.score < score;
		}
};

class Move{
	public:
		static const int EP_FLAG = 0x10000;
		static const int CASTLE_FLAG = 0x200000;
		static const int PAWN_JUMP_FLAG = 0x400000;
		static const int NO_FLAGS = 0x0;
		static const int NO_MOVE = 0x0;
		static const int MAX_LEGAL_MOVES = 256;
		
		static int get_move(int from, int to, int captured, int promoteTo, int isEP, int isPJ, int isCastle);
		static int from(int move);
		static int to(int move);
		static int captured(int move);
		static bool isEP(int move);
		static bool isPJ(int move);
		static bool isCastle(int move);
		static int promoteTo(int move);
		static std::string toNotation(int move);
		static std::string toLongNotation(int move);		
		static void print(int move);
};

class MoveList{
	private:
		int moves[Move::MAX_LEGAL_MOVES];		
		int n = 0;
		
	public:
		void reset(){ n = 0;}
		void add(int move){moves[n++] = move;}
		int get(int i){ return moves[i];}
		void set(int idx, int mv){moves[idx] = mv;}
		int size(){return n;}
		int last(){return moves[n - 1];}
};

#endif