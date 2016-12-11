#ifndef BOARD_STATE
#define BOARD_STATE

class BoardState{
	public:
		static const int WK_CASTLE = 0x4000;
		static const int WQ_CASTLE = 0x8000;
		static const int BK_CASTLE = 0x10000;
		static const int BQ_CASTLE = 0x20000;
		static const int NO_FLAGS = 0x0;
		
		static int new_state(int epSquare, int halfMoves, int turn, int flags);
		static int setEpSquare(int s, int ep);
		static int epSquare(int s);
		static int setHalfMoves(int s, int h);
		static int halfMoves(int s);
		static int setCurrentPlayer(int s, int t);
		static int currentPlayer(int s);
		static bool can_castle_ks(int s, int side);
		static bool can_castle_qs(int s, int side);
		static bool white_can_castle_ks(int s);
		static bool white_can_castle_qs(int s);
		static bool black_can_castle_ks(int s);
		static bool black_can_castle_qs(int s);
		static int castle_key(int s);
		static void print(int s);
			
	private:
		static int CASTLE_KS[2];
		static int CASTLE_QS[2];
		
};

#endif
