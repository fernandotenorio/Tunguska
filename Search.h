#ifndef SEARCH_H
#define SEARCH_H

#include "defs.h"
#include "Move.h"
#include "Board.h"
#include <vector>
#include <chrono>
#include <atomic>

/*
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
typedef std::chrono::duration<std::chrono::system_clock> time_interv;
*/

//Eval type
enum {
    empty = 0,
    lowerbound = 1,
    upperbound = 2,
    exact = 3
};

class SearchInfo{
	public:		
		U64 startTime;
		U64 stopTime;
		int depth;
		int depthSet;	//search to this depth
		bool timeSet;	//search  within this time
		U64 nodes;	//evaluated and visited cnt
		bool stopped;
		int movesToGo;
		bool infinity;
		float fh;
		float fhf;
		int nullCut;
};

class Search{
	private:
		//PV > Capture > Killer > Hist
		static const int PV_BONUS = 2000000;
		static const int PROMO_BONUS = 1500000;
		static const int CAPT_BONUS = 1000000;
		static const int KILLER_BONUS_0 = 900000;
		static const int KILLER_BONUS_1 = 800000;
		static const int COUNTER_BONUS = 750000;

		int counterMoves[14][64];
		int moveAtPly[Board::MAX_DEPTH];
		static int VICTIM_SCORES[14];
		static int MVV_VLA_SCORES[14][14];
		std::vector<MoveScore> moveScore;
		void clearSearch();
		int Quiescence(int alpha, int beta);

	public:
		static const int INFINITE;
		//static const int MATE = 29000;
		static const int ENDGAME_MAT = 1779;
		static int LMR_TABLE[64][64];

		// Thread-aware members
		std::atomic<bool>* sharedStop;  // pointer to shared stop signal (from ThreadPool)
		int threadId;

		// Results readable by ThreadPool after search completes
		int completedDepth;
		int rootBestMove;
		int rootBestScore;

		void stop();
		static void initHeuristics();
		SearchInfo info;
		Board board;
		Search();
		Search(Board board, SearchInfo i);
		static U64 getTime();
		void checkUp(SearchInfo& info);
		void orderMoves(Board& board, MoveList& moves, int pvMove, int counterMove = 0);
		int search(bool verbose);
		int aspirationWindow(Board* board, int depth, int score);
		int alphaBeta(int alpha, int beta, int depth, bool doNull);
		static bool isBadCapture(const Board& board, int move, int side);
		static int see(const Board* board, int toSq, int target, int fromSq, int aPiece);

};



#endif


