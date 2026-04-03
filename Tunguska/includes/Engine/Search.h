#ifndef SEARCH_H
#define SEARCH_H

#include "Board.h"
#include "Move.h"
#include <chrono>
#include <atomic>
#include "NNUE/nnue_loader.h"
#include "Engine/TimeManager.h"

class Search {
public:
    static const int INFINITE = 30000;
    static const int MATE = 29000;
    static const int INVALID_SCORE = -999999;

    // Shared global controls
    static std::atomic<bool> stopped;
    static TimeManager timeManager;
    static std::atomic<long> totalNodes; // To accumulate nodes from all threads

    // Read-only shared tables
    static NNUENetwork nnue_net;
    static int MVV_LVA[14][14];
    static int LMRTable[65][257];

    static void init_search();
    static void stop();
    //static long long currentTimeMillis();

    // --- INSTANCE VARIABLES (Per-Thread) ---
    int threadId;
    long localNodes;
    NNUEState nnue_state;

    int counterMoveTable[64][64];

    Search(int id) : threadId(id), localNodes(0) {
        for (int i = 0; i < 64; ++i) {
            for (int j = 0; j < 64; ++j) {
                counterMoveTable[i][j] = 0;
            }
        }
    }

    // --- NON-STATIC METHODS ---
    void historyStats(Board& board);
    int iterativeDeepening(Board& board, int maxDepth, bool isMainThread);
    int aspirationWindow(Board& board, int depth, int score);

private:
    int alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull, int prevMove);
    int quiescence(Board& board, int alpha, int beta);
    int see(const Board* board, int toSq, int target, int fromSq, int aPiece);
    bool isBadCapture(const Board& board, int move, int side);
    void checkTime();

    int scoreMove(const Board& board, int move, int pvMove, int prevMove);
    void sortMoves(MoveList& moves, const Board& board, int pvMove, int ply, int prevMove);

    inline void updateHistory(int& currentHistory, int bonus) {
        // The max value for history. Must be a power of 2 for some gravity formulas, but 16384 is standard.
        const int MAX_HISTORY = 16384; 
        // This is the "gravity" formula. The score moves towards the bonus, but moves slower as it gets closer to MAX_HISTORY.
        currentHistory += bonus - currentHistory * abs(bonus) / MAX_HISTORY;
    }
};

#endif