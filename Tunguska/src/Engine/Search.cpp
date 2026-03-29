#include "Engine/Search.h"
#include "Engine/Evaluation.h"
#include "Engine/MoveGen.h"
#include "Engine/HashTable.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include "NNUE/FeatureExtractor.h"

std::atomic<bool> Search::stopped{false};
long long Search::timeLimit = -1;
long long Search::startTime = 0;
std::atomic<long> Search::totalNodes{0};

void Search::stop() {
    stopped = true;
}

// Helper to get current time in milliseconds
long long Search::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void Search::checkTime() {
    if (timeLimit != -1) {
        if (currentTimeMillis() - startTime >= timeLimit) {
            stopped = true;
        }
    }
}

// NNUE and tables
NNUENetwork Search::nnue_net;
int Search::MVV_LVA[14][14];
int Search::LMRTable[65][257];

void Search::init_search() {
    int values[] = { 0, 0, 
        Evaluation::PAWN_VAL, Evaluation::PAWN_VAL, Evaluation::KNIGHT_VAL, Evaluation::KNIGHT_VAL,
		Evaluation::BISHOP_VAL, Evaluation::BISHOP_VAL, Evaluation::ROOK_VAL, Evaluation::ROOK_VAL,
		Evaluation::QUEEN_VAL, Evaluation::QUEEN_VAL, Evaluation::KING_VAL, Evaluation::KING_VAL
    };
    for (int v = 2; v < 14; v++) {
        for (int a = 2; a < 14; a++) {
            MVV_LVA[v][a] = values[v] + 10 - (values[a] / 100);
        }
    }

    for (int d = 0; d < 65; d++) {
        for (int m = 0; m < 257; m++) {
            if (d > 0 && m > 0) {
                // Standard Stockfish-like logarithmic formula
                LMRTable[d][m] = static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25);
            } else {
                LMRTable[d][m] = 0;
            }
        }
    }
    // load NNUE weights here if loading from external file
}

void Search::historyStats(Board& board){
    int maxHist = 0;
    long long sumHist = 0;
    int countHist = 0;

    for (int p = 0; p < 14; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            int h = board.searchHistory[p][sq];
            if (h != 0) {
                if (h > maxHist)
                    maxHist = h;
                sumHist += h;
                countHist++;
            }
        }
    }

    double avgHist = (countHist > 0) ? (double)sumHist / countHist : 0.0;
    std::cout << "History stats: max=" << maxHist
          << " avg=" << avgHist
          << " nonzero=" << countHist << std::endl;
}

int Search::iterativeDeepening(Board& board, int maxDepth, long long moveTime, bool isMainThread) {
    nnue_state.init(board);

    localNodes = 0;
    int bestMove = Move::NO_MOVE;
    int score = 0;
    int pvCount = 0;

    // Use maxDepth directly here too
    for (int d = 1; d <= maxDepth; d++) {
        board.ply = 0;
        score = aspirationWindow(board, d, score);

        // Use static stopped flag
        if (Search::stopped) break;

        pvCount = HashTable::getPVLine(d, board);

        // ONLY print to the console if this is the main thread!
        if (isMainThread) {
            std::cout << "info depth " << d << " score cp " << score << " nodes " << Search::totalNodes
                << " time " << (currentTimeMillis() - Search::startTime) << " pv ";

            for (int i = 0; i < pvCount; i++) {
                std::cout << Move::toLongNotation(board.pvArray[i]) << " ";
            }
            std::cout << std::endl;
        }
        
        if (pvCount > 0) {
            bestMove = board.pvArray[0];
        }
        if (score > MATE || score < -MATE) break;
    }

    Search::totalNodes += localNodes;

    if (bestMove == Move::NO_MOVE) {
        MoveList moves;
        MoveGen::pseudoLegalMoves(&board, board.state.currentPlayer, moves, false);

        for (int i = 0; i < moves.size(); i++) {
            BoardState undo = board.makeMove(moves.get(i));
            if (undo.valid) {
                bestMove = moves.get(i);
                board.undoMove(moves.get(i), undo);
                break;
            }
        }
    }

    if (isMainThread){
        Search::stop();
        std::cout << "bestmove " << Move::toLongNotation(bestMove) << std::endl;
    }

    return bestMove;
}

int Search::aspirationWindow(Board& board, int depth, int prevScore) {

    if (depth <= 3)
        return alphaBeta(board, -MATE, MATE, depth, true, Move::NO_MOVE);

    int delta = 15;
    int alpha = std::max(-MATE, prevScore - delta);
    int beta  = std::min(MATE, prevScore + delta);

    while (true) {

        int score = alphaBeta(board, alpha, beta, depth, true, Move::NO_MOVE);

        if (stopped)
            return score;

        if (score <= alpha) {
            alpha = std::max(-MATE, alpha - delta);
        }
        else if (score >= beta) {
            beta = std::min(MATE, beta + delta);
        }
        else {
            return score;
        }

        delta += delta / 2;

        if (delta > 1000) {
            return alphaBeta(board, -MATE, MATE, depth, true, Move::NO_MOVE);
        }
    }
}

static const int FUTIL_MARGIN[4] = {0, 200, 300, 450};
int Search::alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull, int prevMove) {
    // 1. Periodic Resource Check
    if ((localNodes & 1023) == 0) {
        checkTime();
        Search::totalNodes += localNodes;
        localNodes = 0;
    }
    if (stopped) return 0;

    // 2. Draw Detection
    if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0){
        return 0;
    }

    // 3. Max Depth Safety
    if (board.ply >= Board::MAX_DEPTH - 1) {
        return nnue_state.evaluate(board.state.currentPlayer == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);
    }

    localNodes++;
    if (depth <= 0) return quiescence(board, alpha, beta);

    // 4. Hash Table Probe
    int pvMove = Move::NO_MOVE;
    int hashScore = 0;
    if (HashTable::probeHashEntry(board, &pvMove, &hashScore, alpha, beta, depth)) {
        return hashScore;
    }

    // --- Internal Iterative Reductions (IIR) ---
    if (depth >= 4 && pvMove == Move::NO_MOVE) {
        depth-= 1;
    }

    int side = board.state.currentPlayer;
    bool inCheck = MoveGen::isSquareAttacked(&board, board.kingSQ[side], side ^ 1);

    // 5. Static Eval & Futility Pruning
    int staticEval = 0;
    bool futility_prune = false;
    
    if (!inCheck) {
        staticEval = nnue_state.evaluate(side == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);

        if (depth <= 5) {    
            // Reverse Futility Pruning (Static Null Move Pruning)
            if (abs(beta) < MATE - 100) {
                int rfp_margin = 120 * depth; 
                if (staticEval - rfp_margin >= beta) {
                    return staticEval - rfp_margin; 
                }
            }
            
            // Standard Futility Pruning setup
            if (depth <= 3 && abs(alpha) < MATE - 100) {
                if (staticEval + FUTIL_MARGIN[depth] <= alpha) {
                    futility_prune = true;
                }
            }
        }
    }

    // 6. Null Move Pruning
    bool hasBigPiece = (board.bitboards[Board::KNIGHT | side] |
                        board.bitboards[Board::BISHOP | side] |
                        board.bitboards[Board::ROOK   | side] |
                        board.bitboards[Board::QUEEN  | side]) != 0ULL;
    
    if (doNull && !inCheck && hasBigPiece && depth >= 3) {
        int evalMargin = (staticEval - beta) / 200; 
        evalMargin = std::max(0, std::min(2, evalMargin)); 
        int R = 3 + (depth / 4) + evalMargin;
        int nmpDepth = depth - R - 1;

        BoardState undo = board.makeNullMove();
        int score = -alphaBeta(board, -beta, -beta + 1, nmpDepth, false, Move::NO_MOVE);
        board.undoNullMove(undo);
        if (stopped) return 0;
        if (score >= beta) return beta;
    }

    // 7. Move Generation & Sorting
    MoveList moves;
    MoveGen::pseudoLegalMoves(&board, side, moves, inCheck);
    sortMoves(moves, board, pvMove, board.ply, prevMove);

    //@begin change
    // Root Move Randomization for Lazy SMP
    // Rotate move list differently per worker thread to diversify search
    if (board.ply == 0 && threadId > 0) {
        int count = moves.size();

        if (count > 1) {
            int offset = threadId % count;

            if (offset != 0) {
                int first = moves.get(0);

                for (int i = 0; i < offset; i++) {
                    moves.set(i, moves.get(i + 1));
                }

                moves.set(offset, first);
            }
        }
    }
    //@end change

    int legalMovesCount = 0;
    int oldAlpha = alpha;
    int score = -INFINITE;
    int bestScore = -INFINITE;
    int bestMove = Move::NO_MOVE;

    int extension = inCheck ? 1 : 0;
    int counterMove = (prevMove != Move::NO_MOVE) ? counterMoveTable[Move::from(prevMove)][Move::to(prevMove)] : Move::NO_MOVE;

    // 8. Move Loop
    for (int i = 0; i < moves.size(); i++) {
        int move = moves.get(i);
        bool isCounterMove = (move == counterMove);

        FeatureChanges changes = FeatureExtractor::moveDiffFeatures(board, move);
        BoardState undo = board.makeMove(move);

        if (!undo.valid) {
            continue;
        }

        nnue_state.update(changes);

        legalMovesCount++;
        int captured = Move::captured(move);
        int promoted = Move::promoteTo(move);
        bool isEP = Move::isEP(move);
        bool isQuiet = (captured == Board::EMPTY && promoted == Board::EMPTY && !isEP);

        // --- LATE MOVE PRUNING (LMP) ---
        // If we are at a low depth, not in check, and have already searched enough moves...
        if (depth <= 4 && !inCheck && isQuiet) {
            // Formula: Allow more moves at higher depths (e.g., depth 1 = 5 moves, depth 2 = 11 moves)
            int lmpThreshold = 3 + 4 * depth * depth;
            
            if (legalMovesCount > lmpThreshold) {
                // Don't prune killers or PV moves!
                if (move != pvMove && 
                    move != board.searchKillers[0][board.ply] && 
                    move != board.searchKillers[1][board.ply]) {
                    
                    nnue_state.updateUndo(changes);
                    board.undoMove(move, undo);
                    continue; // Skip this move!
                }
            }
        }

        // Futility prune quiet moves at low depth
        if (legalMovesCount > 1 && futility_prune && isQuiet &&
            move != pvMove &&
            move != board.searchKillers[0][board.ply] && 
            move != board.searchKillers[1][board.ply]) {
            
            nnue_state.updateUndo(changes);
            board.undoMove(move, undo);
            continue;
        }

        // --- PVS & LMR LOGIC START ---
        int currentDepth = depth - 1 + extension;

        if (legalMovesCount == 1) {
            // First Move (PV Move) gets a Full Window, Full Depth Search
            score = -alphaBeta(board, -beta, -alpha, currentDepth, true, move);
        } else {
            int reduction = 0;

            // Calculate Table-based Late Move Reduction
            if (depth >= 3 && legalMovesCount > 3 && isQuiet && !inCheck) {
                bool isKiller = (move == board.searchKillers[0][board.ply] || 
                                 move == board.searchKillers[1][board.ply]);
                
                // Fetch base reduction from our pre-calculated log table
                int lmrDepth = std::min(depth, 64);
                int lmrMoves = std::min(legalMovesCount, 256);
                reduction = Search::LMRTable[lmrDepth][lmrMoves];

                // Engine-specific LMR adjustments
                if (isKiller || isCounterMove) reduction--; 
                if (beta - alpha > 1) reduction--; // Reduce less in PV nodes

                int piece = board.board[Move::from(move)];
                int historyScore = board.searchHistory[piece][Move::to(move)];
                reduction -= historyScore / 4096;

                // Clamp reduction to safe bounds
                reduction = std::max(0, reduction);
                if (reduction >= currentDepth) reduction = currentDepth - 1; // Don't drop into Quiescence
                reduction = std::max(0, reduction);
            }

            // PVS Step 1: Null-Window Search (to prove the move is worse than alpha)
            score = -alphaBeta(board, -alpha - 1, -alpha, currentDepth - reduction, true, move);

            // PVS Step 2: Re-search if the move unexpectedly beat alpha
            if (score > alpha) {
                // If the move was reduced, we must re-verify it at full depth (still Null-Window)
                if (reduction > 0) {
                    score = -alphaBeta(board, -alpha - 1, -alpha, currentDepth, true, move);
                }

                // If it STILL beats alpha, AND is less than beta, it's a new Best Move! 
                // We must re-search with the Full Window to get its exact evaluation.
                if (score > alpha && score < beta) {
                    score = -alphaBeta(board, -beta, -alpha, currentDepth, true, move);
                }
            }
        }
        // --- PVS & LMR LOGIC END ---

        nnue_state.updateUndo(changes);
        board.undoMove(move, undo);

        if (stopped) return 0;

        if (score > bestScore){
            bestScore = score;
            bestMove = move;

            if (score > alpha) {
                if (score >= beta) {
                    // This is a Beta Cutoff (a "good" move)
                    HashTable::storeHashEntry(board, bestMove, beta, HFBETA, depth);

                    if (isQuiet) {
                        // 1. Update Killer Moves
                        board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
                        board.searchKillers[0][board.ply] = move;

                        // NEW: Update Countermove Table
                        if (prevMove != Move::NO_MOVE) {
                            counterMoveTable[Move::from(prevMove)][Move::to(prevMove)] = move;
                        }

                        // 2. Calculate the bonus. Cap it to prevent runaway values.
                        int bonus = std::min(depth * depth, 400);

                        // 3. REWARD the move that caused the cutoff
                        int piece = board.board[Move::from(move)];
                        updateHistory(board.searchHistory[piece][Move::to(move)], bonus);

                        // 4. PENALIZE all the quiet moves we searched before this one that FAILED
                        for (int j = 0; j < i; j++) {
                            int penalizedMove  = moves.get(j);
                            int prevCap = Move::captured(penalizedMove);
                            int prevProm = Move::promoteTo(penalizedMove);
                            
                            if (prevCap == Board::EMPTY && prevProm == Board::EMPTY && !Move::isEP(penalizedMove)) {
                                int pPiece = board.board[Move::from(penalizedMove )];
                                updateHistory(board.searchHistory[pPiece][Move::to(penalizedMove)], -bonus);
                            }
                        }
                    }
                    return beta; // Fail-High
                }
                alpha = score;
            }
        }
    } // moves loop

    // 9. Checkmate / Stalemate detection
    if (legalMovesCount == 0) {
        return inCheck ? (-MATE + board.ply) : 0;
    }

    // 10. Hash Table Store
    if (alpha > oldAlpha){
		HashTable::storeHashEntry(board, bestMove, bestScore, HFEXACT, depth);
	} else{		
		HashTable::storeHashEntry(board, bestMove, bestScore, HFALPHA, depth);
	}
    return bestScore;
}

int Search::scoreMove(const Board& board, int move, int pvMove, int prevMove) {
    if (move == pvMove) return 2000000;

    int promo = Move::promoteTo(move);
    if (promo){
        return 1500000 + abs(Evaluation::PIECE_VALUES[promo]);
    }

    if (Move::isEP(move)){
		return MVV_LVA[Board::WHITE_PAWN][Board::BLACK_PAWN] + 1000000;
    }

    int captured = Move::captured(move);

    if (captured != Board::EMPTY) {
        int attacker = board.board[Move::from(move)];
        
        // Get absolute piece values (assuming your PIECE_VALUES might be negative for Black)
        int attackerVal = abs(Evaluation::PIECE_VALUES[attacker]);
        int victimVal = abs(Evaluation::PIECE_VALUES[captured]);

        // OPTIMIZATION: Only run the expensive SEE calculation if the capture looks "risky".
        // A capture is risky if we are using a more valuable piece to take a less valuable piece.
        bool isRisky = attackerVal > victimVal;

        // If it's risky, test it. If it fails SEE, punish its score heavily.
        if (isRisky && isBadCapture(board, move, board.state.currentPlayer)) {
            return -1000000 + MVV_LVA[captured][attacker];
        }
        // If it's NOT risky (e.g. PxQ), OR if it IS risky but passed the SEE test (e.g. RxN but the N was hanging),
        // we score it highly as a good capture.
        return 1000000 + MVV_LVA[captured][attacker];
    }

    // Killer moves
    if (board.searchKillers[0][board.ply] == move) return 900000;
    if (board.searchKillers[1][board.ply] == move) return 800000;

    // NEW: Countermove
    if (prevMove != Move::NO_MOVE && counterMoveTable[Move::from(prevMove)][Move::to(prevMove)] == move) {
        return 750000; // Scored just below killers, heavily above standard history
    }

    // History heuristic
    return board.searchHistory[board.board[Move::from(move)]][Move::to(move)];
}

// TODO: pickNextBest, i.e., lazy sorting.
void Search::sortMoves(MoveList& moves, const Board& board, int pvMove, int ply, int prevMove) {
    int count = moves.size();
    
    // 1. Stack Allocation (Instant)
    // We use a parallel array to store scores. 
    // This lives on the CPU stack, not the heap.
    int scores[Move::MAX_LEGAL_MOVES]; 

    // 2. Score all moves
    for (int i = 0; i < count; ++i) {
        scores[i] = scoreMove(board, moves.get(i), pvMove, prevMove);
    }

    // 3. Selection Sort
    // Finding the best move and swapping it to the front is generally 
    // faster than std::sort for small arrays (N < 50) because it minimizes data movement.
    for (int i = 0; i < count - 1; ++i) {
        int bestIndex = i;
        int bestScore = scores[i];

        // Find the move with the highest score in the remaining list
        for (int j = i + 1; j < count; ++j) {
            if (scores[j] > bestScore) {
                bestScore = scores[j];
                bestIndex = j;
            }
        }

        // Swap if a better move was found
        if (bestIndex != i) {
            // Swap scores in our local array
            scores[bestIndex] = scores[i];
            scores[i] = bestScore;

            // Swap moves in the actual MoveList
            int tempMove = moves.get(i);
            moves.set(i, moves.get(bestIndex));
            moves.set(bestIndex, tempMove);
        }
    }
}

int Search::quiescence(Board& board, int alpha, int beta) {
    assert(alpha < beta);

    // 1. Periodic Resource Check
    if ((localNodes & 1023) == 0) {
        checkTime();
        Search::totalNodes += localNodes;
        localNodes = 0;
    }
    if (stopped) return 0;

    localNodes++;

    // 2. Check for Repetition / 50-move rule
    // Essential now that we allow non-capture evasions (perpetual check detection)
    if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0) {
        return 0;
    }

    // Safety: Prevent stack overflow
    if (board.ply >= Board::MAX_DEPTH - 1) {
        //return Evaluation::evaluate(board);
        return nnue_state.evaluate(board.state.currentPlayer == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);
    }

    // 3. Check State Analysis
    int side = board.state.currentPlayer;
    bool inCheck = MoveGen::isSquareAttacked(&board, board.kingSQ[side], side ^ 1);

    // 4. Stand-Pat (Only if NOT in check)
    int standPat = -Search::INFINITE;

    if (!inCheck) {
        //standPat = Evaluation::evaluate(board);
        standPat = nnue_state.evaluate(board.state.currentPlayer == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);

        if (standPat >= beta) {
            return beta;
        }

        if (standPat > alpha) {
            alpha = standPat;
        }
    }

    // 5. Move Generation
    MoveList moves;
    
    if (inCheck) {
        U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
        MoveGen::getEvasions(&board, side, moves, occup);
    }
    else {
        MoveGen::pseudoLegalCaptureMoves(&board, side, moves);
        MoveGen::pawnPromotions(&board, side, moves, true);
    }

    // 6. Score and Sort Moves
    sortMoves(moves, board, Move::NO_MOVE, board.ply, Move::NO_MOVE);

    int legalMoves = 0;

    for (int i = 0; i < moves.size(); i++) {
        int move = moves.get(i);

        // --- PRUNING (Only when NOT in Check) ---
        if (!inCheck) {
            int promote = Move::promoteTo(move);
            int captured = Move::captured(move);
            bool isEP = Move::isEP(move);

            // handle en passant correctly!
            int capturedValue = isEP ? Evaluation::PIECE_VALUES[Board::WHITE_PAWN] : Evaluation::PIECE_VALUES[captured];

            // Delta Pruning
            if (promote == Board::EMPTY) {
                int delta = abs(capturedValue) + 200;
                if (standPat + delta < alpha) continue;
            }

            // SEE Pruning
            // If it is En Passant, DO NOT use SEE pruning. 
            // SEE struggles with the geometry of En Passant (the captured pawn is on a different square).
            if (promote == Board::EMPTY) {
                int from = Move::from(move);
                int to = Move::to(move);
                int piece = board.board[from];

                // Check if capture loses material
                if (!isEP && see(&board, to, captured, from, piece) < 0) continue;
            }
        }

        FeatureChanges changes = FeatureExtractor::moveDiffFeatures(board, move);
        BoardState undo = board.makeMove(move);

        if (!undo.valid) {            
            continue;
        }

        nnue_state.update(changes);
        
        legalMoves++;
        int score = -quiescence(board, -beta, -alpha);

        nnue_state.updateUndo(changes);
        board.undoMove(move, undo);

        if (stopped) return 0;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    // 7. Checkmate Detection
    if (inCheck && legalMoves == 0) {
        return -Search::MATE + board.ply;
    }

    return alpha;
}

bool Search::isBadCapture(const Board& board, int move, int side) {
    int from = Move::from(move);
    int to = Move::to(move);
    int attacker = board.board[from];
    int target = board.board[to];

    return Search::see(&board, to, target, from, attacker) < 0;
}

U64 getLeastValuablePiece(const Board* board, U64 attadef, int side, int& piece) {
    for (piece = Board::PAWN + side; piece <= Board::KING + side; piece += 2) {
        U64 subset = attadef & board->bitboards[piece];
        if (subset)
            //return subset & -subset; //ok in other compilers
            return subset & (~subset + 1);
    }
    return 0;
}

#include "Engine/Magic.h"
U64 considerXrays(const Board* board, U64 occu, int sq) {
    int color = board->state.currentPlayer;
    U64 rookQueens = board->bitboards[Board::WHITE_ROOK] | board->bitboards[Board::WHITE_QUEEN] |
        board->bitboards[Board::BLACK_ROOK] | board->bitboards[Board::BLACK_QUEEN];

    U64 bishopQueens = board->bitboards[Board::WHITE_BISHOP] | board->bitboards[Board::WHITE_QUEEN] |
        board->bitboards[Board::BLACK_BISHOP] | board->bitboards[Board::BLACK_QUEEN];

    U64 att = (Magic::rookAttacksFrom(occu, sq) & rookQueens) | (Magic::bishopAttacksFrom(occu, sq) & bishopQueens);
    return att & occu;
}

//see gemini3
int Search::see(const Board* board, int toSq, int target, int fromSq, int aPiece) {
    // Array to store score at each depth
    int gain[32];
    int d = 0;

    // Initial gain is the value of the piece sitting on the target square
    gain[d] = abs(Evaluation::PIECE_VALUES[target]);

    int side = board->state.currentPlayer;
    U64 fromSet = (BitBoardGen::ONE << fromSq);

    // All pieces on the board
    U64 occup = board->bitboards[Board::WHITE] | board->bitboards[Board::BLACK];

    // Calculate initial attackers
    U64 attadef = MoveGen::attackers_to(board, toSq, Board::WHITE)
        | MoveGen::attackers_to(board, toSq, Board::BLACK);

    // X-Ray optimizations (update these only when necessary)
    do {
        d++;

        // 1. Calculate gain for this depth
        // value of the piece that just moved - previous gain
        gain[d] = abs(Evaluation::PIECE_VALUES[aPiece]) - gain[d - 1];

        // 2. Pruning
        // If the side to move is already losing material even if they stop now, 
        // they will just stand pat. We can cut off here.
        if (std::max(-gain[d - 1], gain[d]) < 0) {
            break;
        }

        // 3. Update Board State (Simulation)
        attadef ^= fromSet; // Remove the attacker from the list
        occup ^= fromSet;   // Remove the piece from the board

        // 4. Update X-Rays
        // CRITICAL FIX: We generally always check for X-rays because ANY piece can block.
        // Optimization: Only check if the piece that moved is aligned with the target.
        // If you have LINES_BB initialized:
         if (BitBoardGen::LINES_BB[fromSq][toSq]) {
            attadef |= considerXrays(board, occup, toSq);
         }
        // If not, just call considerXrays unconditionally. It is safer.

        // 5. Switch Side & Find Next Attacker
        side ^= 1;

        // Pass aPiece by reference (&). getLeastValuablePiece will update it 
        // to the type of the NEXT attacker (e.g., from Pawn to Rook).
        fromSet = getLeastValuablePiece(board, attadef, side, aPiece);

        // 'fromSq' is only needed if you use LINES_BB optimization above
        fromSq = numberOfTrailingZeros(fromSet); 

    } while (fromSet);

    // Propagate minimax scores back to the root
    while (--d) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    }
    return gain[0];
}
