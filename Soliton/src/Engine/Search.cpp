#include "Engine/Search.h"
#include "Engine/Evaluation.h"
#include "Engine/MoveGen.h"
#include "Engine/HashTable.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include "FeatureExtractor.h"

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

// MVV-LVA table
int Search::MVV_LVA[14][14];

// NNUE
NNUENetwork Search::nnue_net;
NNUEState Search::nnue_state;

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

    // NNUE
    NNUENetwork::loadWeights("D:\\cpp_projs\\Soliton\\Soliton\\weights\\net_7.npz");
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

int Search::iterativeDeepening(Board& board, int maxDepth, long long moveTime, bool verbose) {
    nnue_state.init(board);

    params.nodes = 0;
    params.stopped = false;
    params.bestMove = Move::NO_MOVE;
    params.startTime = currentTimeMillis();
    params.timeLimit = moveTime;
    params.depthLimit = maxDepth;

    int alpha = -INFINITE;
    int beta = INFINITE;
    int score = 0;

    for (int d = 1; d <= params.depthLimit; d++) {
        board.ply = 0;  // not necessary, defensive. Every iterativeDeepening call starts with a fresh board
        score = aspirationWindow(board, d, score);

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
    nnue_state.init(board);

    params.nodes = 0;
    params.stopped = false;
    params.bestMove = Move::NO_MOVE;
    params.startTime = currentTimeMillis();
    params.timeLimit = moveTime;
    params.depthLimit = maxDepth;

    int alpha = -INFINITE;
    int beta = INFINITE;
    int finalScore = INVALID_SCORE;
    int score = 0;

    for (int d = 1; d <= params.depthLimit; d++) {
        board.ply = 0;
        score = aspirationWindow(board, d, score);

        if (params.stopped) break;
        finalScore = score;

        if (score > MATE || score < -MATE) break;
    }
    return finalScore;
}

int Search::aspirationWindow(Board& board, int depth, int prevScore) {

    if (depth <= 3)
        return alphaBeta(board, -MATE, MATE, depth, true);

    int delta = 15;
    int alpha = std::max(-MATE, prevScore - delta);
    int beta  = std::min(MATE, prevScore + delta);

    while (true) {

        int score = alphaBeta(board, alpha, beta, depth, true);

        if (params.stopped)
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
            return alphaBeta(board, -MATE, MATE, depth, true);
        }
    }
}

static const int FUTIL_MARGIN[4] = {0, 200, 300, 450};
int Search::alphaBeta(Board& board, int alpha, int beta, int depth, bool doNull) {
    // Check time every 2048 nodes to avoid overhead of system clock calls
    if ((params.nodes & 2047) == 0) checkTime();
    if (params.stopped) return 0;

    if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0){
        return 0;
    }

    // Safety check for search depth to prevent stack overflow in extreme tactical scenarios
    if (board.ply >= Board::MAX_DEPTH - 1) {
        //return Evaluation::evaluate(board);
        return nnue_state.evaluate(board.state.currentPlayer == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);
    }

    params.nodes++;
    if (depth <= 0) return quiescence(board, alpha, beta);

    int pvMove = Move::NO_MOVE;
    int hashScore = 0;
    if (HashTable::probeHashEntry(board, &pvMove, &hashScore, alpha, beta, depth)) {
        return hashScore;
    }

    int side = board.state.currentPlayer;
    // Determine if we are currently in check (Evasion)
    bool inCheck = MoveGen::isSquareAttacked(&board, board.kingSQ[side], side ^ 1);

    // Static Eval for Futility Pruning
    int staticEval = 0;
    bool futility_prune = false;
    
    if (!inCheck) {
        // We calculate staticEval once if we are at low depths (RFP goes up to depth 5)
        if (depth <= 5) {
            //staticEval = Evaluation::evaluate(board);
            staticEval = nnue_state.evaluate(side == Board::WHITE ? WHITE_NNUE : BLACK_NNUE);
            
            // 1. Reverse Futility Pruning (Static Null Move Pruning)
            // If we are not in a mate sequence, and the position is so good that 
            // even after subtracting a safety margin we still beat beta, we prune!
            if (abs(beta) < MATE - 100) {
                int rfp_margin = 120 * depth; 
                if (staticEval - rfp_margin >= beta) {
                    return staticEval - rfp_margin; // Cause a Beta cutoff immediately
                }
            }
            
            // 2. Standard Futility Pruning setup
            if (depth <= 3 && abs(alpha) < MATE - 100) {
                if (staticEval + FUTIL_MARGIN[depth] <= alpha) {
                    futility_prune = true;
                }
            }
        }
    }

    // Null Move Pruning
    bool hasBigPiece = (board.bitboards[Board::KNIGHT | side] |
                        board.bitboards[Board::BISHOP | side] |
                        board.bitboards[Board::ROOK   | side] |
                        board.bitboards[Board::QUEEN  | side]) != 0ULL;
    int R = 2 + depth/4;
   
    if (doNull && !inCheck && hasBigPiece && depth > R) {
        BoardState undo = board.makeNullMove();
        int score = -alphaBeta(board, -beta, -beta + 1, depth - R - 1, false);
        board.undoNullMove(undo);
        if (params.stopped) return 0;
        if (score >= beta) return beta;
    }

    MoveList moves;
    MoveGen::pseudoLegalMoves(&board, side, moves, inCheck);
    sortMoves(moves, board, pvMove, board.ply);

    int legalMovesCount = 0;
    int oldAlpha = alpha;
    int score = -INFINITE;
    int bestScore = -INFINITE;
    int bestMove = Move::NO_MOVE;

    // If we are in check, we extend the search by 1 ply to resolve the threat.
    int extension = 0;
    if (inCheck) extension = 1;

    for (int i = 0; i < moves.size(); i++) {
        int move = moves.get(i);

        FeatureChanges changes = FeatureExtractor::moveDiffFeatures(board, move);
        nnue_state.update(changes);
        BoardState undo = board.makeMove(move);

        if (!undo.valid) {
            nnue_state.updateUndo(changes);
            continue;
        }

        legalMovesCount++;
        //int oppKingSQ = board.kingSQ[board.state.currentPlayer];
        //bool giveCheck = MoveGen::isSquareAttacked(&board, oppKingSQ, board.state.currentPlayer^1);

        // Futility prune quiet moves at low depth if static eval + margin is still below alpha
        if (legalMovesCount > 1 && futility_prune &&
            move != pvMove &&
            move != board.searchKillers[0][board.ply] && 
            move != board.searchKillers[1][board.ply]) {
            
            int captured = Move::captured(move);
            int promoted = Move::promoteTo(move);
            
            if (captured == Board::EMPTY && promoted == Board::EMPTY) {
                nnue_state.updateUndo(changes);
                board.undoMove(move, undo);
                continue;
            }
        }

        // LATE MOVE REDUCTION (LMR)
        int reduction = 0;
        
        // Conditions for reduction:
        // 1. Depth is substantial (> 2)
        // 2. We have searched the best moves already (legalMovesCount > 3)
        // 3. We are NOT in check (do not reduce evasions)
        if (depth > 3 && legalMovesCount > 3 && !inCheck) {
            
            int captured = Move::captured(move);
            int promoted = Move::promoteTo(move);

            // 4. Move is quiet (no capture, no promotion)
            if (captured == Board::EMPTY && promoted == Board::EMPTY) {
                
                // 5. Move is not a Killer move
                if (move != board.searchKillers[0][board.ply] && 
                    move != board.searchKillers[1][board.ply]) {
                    
                    reduction = 1;
                    // Reduce more for very late moves at high depth
                    if (legalMovesCount > 6) reduction = 2;
                    
                    // Safety clamp
                    if (reduction >= depth - 1) reduction = depth - 2;
                }
            }
        }

        // Calculate final depth for this move
        int currentDepth = depth - 1 + extension - reduction;

        score = -alphaBeta(board, -beta, -alpha, currentDepth, true);

        // Re-Search Logic:
        // If we reduced the depth, and the move beat alpha (it was better than we thought),
        // we must search it again at full depth to get the accurate score.
        if (reduction > 0 && score > alpha) {
            currentDepth = depth - 1 + extension; // Full depth (with extension, no reduction)
            score = -alphaBeta(board, -beta, -alpha, currentDepth, true);
        }

        nnue_state.updateUndo(changes);
        board.undoMove(move, undo);

        if (params.stopped) return 0;

        if (score > bestScore){
            bestScore = score;
            bestMove = move;

            if (score > alpha) {
                if (score >= beta) {
                    if (Move::captured(move) == Board::EMPTY) {
                        board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
                        board.searchKillers[0][board.ply] = move;

                        // History heuristic
                        int piece = board.board[Move::from(move)];
                        board.searchHistory[piece][Move::to(move)] += depth * depth;
                    }
                    HashTable::storeHashEntry(board, bestMove, beta, HFBETA, depth);
                    return beta;
                }
                alpha = score;
            }
        }
    } // moves loop

    if (legalMovesCount == 0) {
        return inCheck ? (-MATE + board.ply) : 0;
    }

    if (alpha > oldAlpha){
		HashTable::storeHashEntry(board, bestMove, bestScore, HFEXACT, depth);
	} else{		
		HashTable::storeHashEntry(board, bestMove, bestScore, HFALPHA, depth);
	}
    return bestScore;
}

int Search::scoreMove(const Board& board, int move, int pvMove) {
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

        FeatureChanges changes = FeatureExtractor::moveDiffFeatures(board, move);
		nnue_state.update(changes);
        BoardState undo = board.makeMove(move);

        if (!undo.valid) {
            nnue_state.updateUndo(changes);
            continue;
        }

        legalMoves++;

        int score = -quiescence(board, -beta, -alpha);

        nnue_state.updateUndo(changes);
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
