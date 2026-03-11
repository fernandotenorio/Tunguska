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

    // Shared global controls
    static std::atomic<bool> stopped;
    static long long timeLimit;
    static long long startTime;
    static std::atomic<long> totalNodes; // To accumulate nodes from all threads

    // Read-only shared tables
    static NNUENetwork nnue_net;
    static int MVV_LVA[14][14];
    static int LMRTable[65][257];

    static void init_search();
    static void stop();
    static long long currentTimeMillis();

    // --- INSTANCE VARIABLES (Per-Thread) ---
    int threadId;
    long localNodes;
    NNUEState nnue_state;

    Search(int id) : threadId(id), localNodes(0) {}

    // --- NON-STATIC METHODS ---
    void historyStats(Board& board);
    int iterativeDeepening(Board& board, int maxDepth, long long moveTime, bool isMainThread);
    int iterativeDeepeningScore(Board& board, int maxDepth, long long moveTime, bool isMainThread);
    int aspirationWindow(Board& board, int depth, int score);

private:
    int alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull);
    int quiescence(Board& board, int alpha, int beta);
    int see(const Board* board, int toSq, int target, int fromSq, int aPiece);
    bool isBadCapture(const Board& board, int move, int side);
    void checkTime();

    int scoreMove(const Board& board, int move, int pvMove);
    void sortMoves(MoveList& moves, const Board& board, int pvMove, int ply);
};

#endif