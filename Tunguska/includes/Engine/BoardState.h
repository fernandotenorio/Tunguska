#ifndef BOARD_STATE
#define BOARD_STATE

#include "defs.h"

class BoardState{
	public:
		int epSquare;
		int halfMoves;
		int currentPlayer;
		//1111 -> black_sq, black_ks, white_qs, white_ks
		int castleKey;
		U64 zKey;
		bool valid;

		static const int CASTLE_ALL = 15;
		static const int WK_CASTLE = 1;
		static const int WQ_CASTLE = 2;
		static const int W_CASTLE_BOTH = 3;
		static const int BK_CASTLE = 4;
		static const int BQ_CASTLE = 8;
		static const int B_CASTLE_BOTH = 12;

		bool can_castle_ks(int side);
		bool can_castle_qs(int side);
		bool white_can_castle_ks();
		bool white_can_castle_qs();
		bool black_can_castle_ks();
		bool black_can_castle_qs();

		/*
		void set_white_castle_ks();
		void set_white_castle_qs();
		void set_black_castle_ks();
		void set_black_castle_qs();
		*/

		BoardState(){
			epSquare = 0;
			halfMoves = 0;
			currentPlayer = 0;
			castleKey = 0;
			zKey = 0;
			valid = false;
		}

		BoardState(int eps, int hm, int cp, int ck, U64 zk){
			epSquare = eps;
			halfMoves = hm;
			currentPlayer = cp;
			castleKey = ck;
			zKey = zk;
			valid = false;
		}

		BoardState(const BoardState& s){
			epSquare = s.epSquare;
			halfMoves = s.halfMoves;
			currentPlayer = s.currentPlayer;
			castleKey = s.castleKey;
			zKey = s.zKey;
			valid = s.valid;
		}

	private:
		static int CASTLE_KS[2];
		static int CASTLE_QS[2];
};

#endif