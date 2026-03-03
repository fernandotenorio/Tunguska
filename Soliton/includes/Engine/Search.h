#ifndef SEARCH_H
#define SEARCH_H

#include "Board.h"
#include "Move.h"
#include <chrono>
#include <atomic>
#include "nnue_loader.h"

class Search {
public:
    static const int INFINITE = 30000;
    static const int MATE = 29000;
    static const int INVALID_SCORE = -999999;

    struct SearchParams {
        long nodes;
        int bestMove;
        int depthLimit;
        long long timeLimit; // in milliseconds
        long long startTime;
        std::atomic<bool> stopped;
    };

    // NNUE
    static NNUENetwork nnue_net;
    static NNUEState nnue_state;

    // MVV
    static int MVV_LVA[14][14];

    static void historyStats(Board& board);
    static void init_search();

    // Updated entry point
    static int iterativeDeepening(Board& board, int maxDepth, long long moveTime, bool verbose);
    //For eval FEN tool
    static int iterativeDeepeningScore(Board& board, int maxDepth, long long moveTime, bool verbose);
    static int aspirationWindow(Board& board, int depth, int score);
    static void stop();

private:
    static int alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull);
    static int quiescence(Board& board, int alpha, int beta);
    static int see(const Board* board, int toSq, int target, int fromSq, int aPiece);
    static bool isBadCapture(const Board& board, int move, int side);
    static void checkTime(); // Checks if we should stop the search

    static int scoreMove(const Board& board, int move, int pvMove);
    static void sortMoves(MoveList& moves, const Board& board, int pvMove, int ply);

    static SearchParams params;
};

#endif