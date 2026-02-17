#include "Engine/Search.h"
#include "Engine/Evaluation.h"
#include "Engine/MoveGen.h"
#include "Engine/HashTable.h"
#include <iostream>
#include <algorithm>
#include <vector>

Search::SearchParams Search::params;

void Search::stop() {
    params.stopped = true;
}

// Helper to get current time in milliseconds
long long currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void Search::checkTime() {
    if (params.timeLimit != -1) {
        if (currentTimeMillis() - params.startTime >= params.timeLimit) {
            params.stopped = true;
        }
    }
}

// MVV-LVA table [Victim][Attacker]
static int MVV_LVA[14][14];
static bool mvv_init = false;

void init_mvv() {
    if (mvv_init) return;
    int values[] = { 0, 0, 100, 100, 200, 200, 300, 300, 400, 400, 500, 500, 600, 600 };
    for (int v = 2; v < 14; v++) {
        for (int a = 2; a < 14; a++) {
            MVV_LVA[v][a] = values[v] + 10 - (values[a] / 100);
        }
    }
    mvv_init = true;
}

int Search::iterativeDeepening(Board& board, int maxDepth, long long moveTime, bool verbose) {
    init_mvv();
    params.nodes = 0;
    params.stopped = false;
    params.bestMove = Move::NO_MOVE;
    params.startTime = currentTimeMillis();
    params.timeLimit = moveTime;
    params.depthLimit = maxDepth;

    int alpha = -INFINITE;
    int beta = INFINITE;

    for (int d = 1; d <= params.depthLimit; d++) {
        board.ply = 0;
        int score = alphaBeta(board, alpha, beta, d, true);

        // If search was stopped during this depth, don't use the results
        if (params.stopped) break;

        int pvCount = HashTable::getPVLine(d, board);

        if (verbose) {
            std::cout << "info depth " << d << " score cp " << score << " nodes " << params.nodes
                << " time " << (currentTimeMillis() - params.startTime) << " pv ";

            for (int i = 0; i < pvCount; i++) {
                std::cout << Move::toLongNotation(board.pvArray[i]) << " ";
            }
            std::cout << std::endl;
        }
        params.bestMove = board.pvArray[0];
        if (score > MATE || score < -MATE) break;
    }

    // Final output: ensure we output a bestmove even if search was stopped
    if (params.bestMove == Move::NO_MOVE) {
        // Fallback: just get any legal move if something went wrong
        MoveList moves;
        MoveGen::pseudoLegalMoves(&board, board.state.currentPlayer, moves, false);
        for (int i = 0; i < moves.size(); i++) {
            BoardState undo = board.makeMove(moves.get(i));
            if (undo.valid) {
                params.bestMove = moves.get(i);
                board.undoMove(moves.get(i), undo);
                break;
            }
        }
    }

    if (verbose)
        std::cout << "bestmove " << Move::toLongNotation(params.bestMove) << std::endl;
    return params.bestMove;
}

int Search::iterativeDeepeningScore(Board& board, int maxDepth, long long moveTime, bool verbose) {
    init_mvv();
    params.nodes = 0;
    params.stopped = false;
    params.bestMove = Move::NO_MOVE;
    params.startTime = currentTimeMillis();
    params.timeLimit = moveTime;
    params.depthLimit = maxDepth;

    int alpha = -INFINITE;
    int beta = INFINITE;
    int finalScore = INVALID_SCORE;

    for (int d = 1; d <= params.depthLimit; d++) {
        board.ply = 0;
        int score = alphaBeta(board, alpha, beta, d, true);

        if (params.stopped) break;
        finalScore = score;

        if (score > MATE || score < -MATE) break;
    }
    return finalScore;
}

int Search::alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull) {
    // Check time every 2048 nodes to avoid overhead of system clock calls
    if ((params.nodes & 2047) == 0) checkTime();
    if (params.stopped) return 0;

    if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0){
        return 0;
    }

    // Safety check for search depth to prevent stack overflow in extreme tactical scenarios
    if (board.ply >= Board::MAX_DEPTH - 1) {
        return Evaluation::evaluate(board);
    }

    params.nodes++;
    if (depth <= 0) return quiescence(board, alpha, beta);

    int pvMove = Move::NO_MOVE;
    int hashScore = 0;
    if (HashTable::probeHashEntry(board, &pvMove, &hashScore, alpha, beta, depth)) {
        return hashScore;
    }

    int side = board.state.currentPlayer;
    bool inCheck = MoveGen::isSquareAttacked(&board, board.kingSQ[side], side ^ 1);

    if (doNull && !inCheck && depth >= 3 && board.material[side] > 500) {
        BoardState undo = board.makeNullMove();
        int score = -alphaBeta(board, -beta, -beta + 1, depth - 3, false);
        board.undoNullMove(undo);
        if (params.stopped) return 0;
        if (score >= beta) return beta;
    }

    MoveList moves;
    MoveGen::pseudoLegalMoves(&board, side, moves, inCheck);
    sortMoves(moves, board, pvMove, board.ply);

    int legalMovesCount = 0;
    int oldAlpha = alpha;
    int bestMove = Move::NO_MOVE;

    for (int i = 0; i < moves.size(); i++) {
        int move = moves.get(i);
        BoardState undo = board.makeMove(move);
        if (!undo.valid) continue;

        legalMovesCount++;
        int score = -alphaBeta(board, -beta, -alpha, depth - 1, true);
        board.undoMove(move, undo);

        if (params.stopped) return 0;

        if (score >= beta) {
            if (Move::captured(move) == Board::EMPTY) {
                board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
                board.searchKillers[0][board.ply] = move;

                 // History heuristic
                int piece = board.board[Move::from(move)];
                board.searchHistory[piece][Move::to(move)] += depth * depth;
            }
            HashTable::storeHashEntry(board, move, beta, HFBETA, depth);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }

    if (legalMovesCount == 0) {
        return inCheck ? (-MATE + board.ply) : 0;
    }

    int flag = (alpha > oldAlpha) ? HFEXACT : HFALPHA;
    HashTable::storeHashEntry(board, bestMove, alpha, flag, depth);
    return alpha;
}

int Search::scoreMove(const Board& board, int move, int pvMove) {
    if (move == pvMove) return 2000000;

    int captured = Move::captured(move);
    if (captured != Board::EMPTY) {
        int attacker = board.board[Move::from(move)];
        return 1000000 + MVV_LVA[captured][attacker];
    }

    // Killer moves
    if (board.searchKillers[0][board.ply] == move) return 900000;
    if (board.searchKillers[1][board.ply] == move) return 800000;

    // History heuristic
    return board.searchHistory[board.board[Move::from(move)]][Move::to(move)];
}

// TODO: pickNextBest, i.e., lazy sorting.
void Search::sortMoves(MoveList& moves, const Board& board, int pvMove, int ply) {
    int count = moves.size();
    
    // 1. Stack Allocation (Instant)
    // We use a parallel array to store scores. 
    // This lives on the CPU stack, not the heap.
    int scores[Move::MAX_LEGAL_MOVES]; 

    // 2. Score all moves
    for (int i = 0; i < count; ++i) {
        scores[i] = scoreMove(board, moves.get(i), pvMove);
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
    if ((params.nodes & 2047) == 0) checkTime();
    if (params.stopped) return 0;

    params.nodes++;

    // 2. Check for Repetition / 50-move rule
    // Essential now that we allow non-capture evasions (perpetual check detection)
    if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0) {
        return 0;
    }

    // Safety: Prevent stack overflow
    if (board.ply >= Board::MAX_DEPTH - 1) {
        return Evaluation::evaluate(board);
    }

    // 3. Check State Analysis
    int side = board.state.currentPlayer;
    bool inCheck = MoveGen::isSquareAttacked(&board, board.kingSQ[side], side ^ 1);

    // 4. Stand-Pat (Only if NOT in check)
    int standPat = -Search::INFINITE;

    if (!inCheck) {
        standPat = Evaluation::evaluate(board);

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
    sortMoves(moves, board, Move::NO_MOVE, board.ply);

    int legalMoves = 0;

    for (int i = 0; i < moves.size(); i++) {
        int move = moves.get(i);

        // --- PRUNING (Only when NOT in Check) ---
        if (!inCheck) {
            int promote = Move::promoteTo(move);
            int captured = Move::captured(move);

            // Delta Pruning
            if (promote == Board::EMPTY) {
                int delta = abs(Evaluation::PIECE_VALUES[captured]) + 200;
                if (standPat + delta < alpha) continue;
            }

            // SEE Pruning
            if (promote == Board::EMPTY) {
                int from = Move::from(move);
                int to = Move::to(move);
                int piece = board.board[from];
                // Check if capture loses material
                if (see(&board, to, captured, from, piece) < 0) continue;
            }
        }

        BoardState undo = board.makeMove(move);
        if (!undo.valid) continue;

        legalMoves++;

        int score = -quiescence(board, -beta, -alpha);
        board.undoMove(move, undo);

        if (params.stopped) return 0;

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
U64 considerXrays(const Board* board, U64 occu, U64 attackdef, int sq) {
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
            attadef |= considerXrays(board, occup, attadef, toSq);
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
