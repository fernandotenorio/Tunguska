#include "Search.h"
#include <iostream>
#include "MoveGen.h"
#include "Move.h"
#include "Evaluation.h"
#include <algorithm>
#include "HashTable.h"


const int Search::INFINITE = 30000;
static const int MATE_SCORE = 29999;

int Search::VICTIM_SCORES[14] = {
		0, 0, 
		Evaluation::PAWN_VAL, Evaluation::PAWN_VAL, Evaluation::KNIGHT_VAL, Evaluation::KNIGHT_VAL,
		Evaluation::BISHOP_VAL, Evaluation::BISHOP_VAL, Evaluation::ROOK_VAL, Evaluation::ROOK_VAL,
		Evaluation::QUEEN_VAL, Evaluation::QUEEN_VAL, Evaluation::KING_VAL, Evaluation::KING_VAL
		};

int Search::MVV_VLA_SCORES[14][14];
		
void Search::initHeuristics(){
	for (int v = 0; v < 14; v++){
		for (int a = 0; a < 14; a++){
			MVV_VLA_SCORES[v][a] = VICTIM_SCORES[v] + 6 - (VICTIM_SCORES[a]/100);
		}
	}
}

Search::Search(){}

Search::Search(Board b, SearchInfo i){
	board = b;
	info = i;
}

void Search::stop(){
	info.stopped = true;
}

U64 Search::getTime(){
	U64 t = (U64)(std::chrono::system_clock::now().time_since_epoch()/std::chrono::milliseconds(1));
	return t;
}

void Search::checkUp(SearchInfo& info){
	if (info.timeSet && (Search::getTime() > info.stopTime)){
		info.stopped = true;
	}
}

std::vector<MoveScore> Search::moveScore(Move::MAX_LEGAL_MOVES);
void Search::orderMoves(Board& board, MoveList& moves, int pvMove){

	int side = board.state.currentPlayer;

	for (int i = 0; i < moves.size(); i++){
			int mv = moves.get(i);
			int capt = Move::captured(mv);
			int promo = Move::promoteTo(mv);

		if (capt > 0){
			int from = Move::from(mv);
			int attacker = board.board[from];
			moveScore[i] = MoveScore(mv, MVV_VLA_SCORES[capt][attacker] + CAPT_BONUS);
		}
		else if (Move::isEP(mv)){
			moveScore[i] = MoveScore(mv, MVV_VLA_SCORES[Board::WHITE_PAWN][Board::BLACK_PAWN] + CAPT_BONUS);
		}
		else{
			if (board.searchKillers[0][board.ply] == mv){
				moveScore[i] = MoveScore(mv, KILLER_BONUS_0);
			} else if (board.searchKillers[1][board.ply] == mv){
				moveScore[i] = MoveScore(mv, KILLER_BONUS_1);
			}else{
				int piece = board.board[Move::from(mv)];
				int to = Move::to(mv);
				moveScore[i] = MoveScore(mv, board.searchHistory[piece][to]);

				//test piece squares ordering
				// if (moveScore[i].score == 0){					
				// 	if (side == Board::WHITE)
				// 		moveScore[i].score = Evaluation::PIECE_SQUARES_END[piece][to];
				// 	else
				// 		moveScore[i].score = Evaluation::PIECE_SQUARES_END[piece][Evaluation::MIRROR64[to]];
				// }
			}
		}

		if (promo){
			moveScore[i].score = PROMO_BONUS + abs(Evaluation::PIECE_VALUES[promo]);
		}

		//see
		/*
		if (false && capt > 0){
			int from = Move::from(mv);	
			int to = Move::to(mv);	
			int attacker = board.board[from];
			int target = board.board[to];

			int seeScore = Search::see(&board, to, target, from, attacker);
			if (seeScore < 0)
				moveScore[i].score = seeScore;

		}
		*/

		//PV override
		if (mv == pvMove)
			moveScore[i].score = PV_BONUS;
	}
	
	std::sort(moveScore.begin(), moveScore.begin() + moves.size(), std::less<MoveScore>());
	
	for (int i = 0; i < moves.size(); i++){
		moves.set(i, moveScore[i].move);
	}
}

void Search::clearSearch(){
	for (int i = 0; i < 14; i++){
		for (int j = 0; j < 64; j++){
			board.searchHistory[i][j] = 0;
		}
	}

	//clear killers
	for (int i = 0; i < 2; i++){
		for (int j = 0; j < Board::MAX_DEPTH; j++){
			board.searchKillers[i][j] = 0;
		}
	}

	board.hashTable->overWrite = 0;
	board.hashTable->hit = 0;	
	board.hashTable->cut = 0;	
	board.ply = 0;
	
	info.stopped = false;
	info.nodes = 0;
	info.fh = 0.0f;
	info.fhf = 0.0f;
}

int Search::search(bool verbose){
	int bestMove = Move::NO_MOVE;
	int bestScore = 0;
	int pvMoves = 0;
	
	clearSearch();

	//iterative deepening
	for (int currentDepth = 1; currentDepth <= info.depth; currentDepth++){
		//bestScore = alphaBeta(-INFINITE, INFINITE, currentDepth, true);
		bestScore = aspirationWindow(&board, currentDepth, bestScore);

		//check stop
		if (info.stopped){
			break;
		}

		pvMoves = HashTable::getPVLine(currentDepth, board);
		bestMove = board.pvArray[0];
		
		if (verbose){
			printf("info score cp %d depth %d nodes %llu time %llu ",
					bestScore, currentDepth, info.nodes, Search::getTime() - info.startTime);
			printf("pv");
			
			for (int i = 0; i < pvMoves; i++){
				int m = board.pvArray[i];
				std::cout << " " <<  Move::toLongNotation(m);
			}
			printf("\n");
		}

		// printf("Hits: %d  Overwrite: %d  NewWrite: %d  Cut: %d\nOrdering: %.2f  NullCut:%d\n",
		// 	board.hashTable->hit, board.hashTable->overWrite, board.hashTable->newWrite, board.hashTable->cut,
		// 	(info.fhf/info.fh)*100, info.nullCut);
	}

	if (verbose)
		std::cout << "bestmove " << Move::toLongNotation(bestMove) << std::endl;
	return bestMove;
}

int Search::aspirationWindow(Board* board, int depth, int prevScore) {

    if (depth <= 3)
        return alphaBeta(-MATE_SCORE, MATE_SCORE, depth, true);

    int delta = 15;
    int alpha = std::max(-MATE_SCORE, prevScore - delta);
    int beta  = std::min(MATE_SCORE, prevScore + delta);

    while (true) {

        int score = alphaBeta(alpha, beta, depth, true);

        if (params.stopped)
            return score;

        if (score <= alpha) {
            alpha = std::max(-MATE_SCORE, alpha - delta);
        }
        else if (score >= beta) {
            beta = std::min(MATE_SCORE, beta + delta);
        }
        else {
            return score;
        }

        delta += delta / 2;

        if (delta > 1000) {
            return alphaBeta(-MATE_SCORE, MATE_SCORE, depth, true);
        }
    }
}

static const int FUTIL_MARGIN[4] = {0, 200, 300, 500};
static const int RAZOR_MARGIN[4] = {0, 325, 345, 395};
int Search::alphaBeta(int alpha, int beta, int depth, bool doNull){

	if ((info.nodes & 2047) == 0){
		checkUp(info);
	}
	
	if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply){	
		return 0;
	}

	if (board.ply >= Board::MAX_DEPTH - 1){		
		return Evaluation::evaluate(board, board.state.currentPlayer);
	}

	//Check extension
	int side = board.state.currentPlayer;
	int opp = side^1;
	int kingSQ = board.kingSQ[side];
	int oppKingSQ = board.kingSQ[opp];
	bool atCheck = MoveGen::isSquareAttacked(&board, kingSQ, opp);

	if (atCheck)
		depth++;

	if (depth <= 0){		
		return Quiescence(alpha, beta);
	}

	info.nodes++;
	int score = -INFINITE;
	int pvMove = Move::NO_MOVE;

	if(HashTable::probeHashEntry(board, &pvMove, &score, alpha, beta, depth)){
		board.hashTable->cut++;
		return score;
	}

	//Eval pruning 
	int static_eval = 0;
	bool static_set = false;	

	if (!pvMove && depth < 3 && !atCheck && abs(beta - 1) > -INFINITE + 100){
		static_eval =  Evaluation::evaluate(board, side);
		static_set = true;
		int eval_margin = 120 * depth;

		if (static_eval - eval_margin >= beta)
			return static_eval - eval_margin;			
	}

	//null move pruning
	bool hasBigPiece = board.material[side] > Search::ENDGAME_MAT;
	int R = 2 + depth/6;

	if(doNull && !atCheck && board.ply > 0 && hasBigPiece && depth > R) {
		BoardState undo_null = board.makeNullMove();
		score = -alphaBeta(-beta, -beta + 1, depth - R - 1, false);
		board.undoNullMove(undo_null);

		if(info.stopped) 
			return 0;
		
		if (score >= beta && abs(score) < ISMATE){
			info.nullCut++;
		  	return beta;
		}
	}

	/*
	int ReverseFutilityStep = 90;
	//Reverse futility pruning
    if(!pvMove && !atCheck && depth <= 7 && static_eval - ReverseFutilityStep * depth > beta) {
        return static_eval;
    }
    */

	//Razoring pruning
	/*
	if (pvMove == Move::NO_MOVE && depth <= 3 && !atCheck && doNull){
		static_eval = static_set ? static_eval : Evaluation::evaluate(board, side);
		static_set = true;

		//ou
		int RazorMargin = 300;		
		if (static_eval + RazorMargin * depth < alpha){
			return Quiescence(alpha, beta);			
		}
		
		//ou
		int threshold = alpha - 300 - (depth - 1) * 60;
		if (static_eval < threshold){
			int val = Quiescence(alpha, beta);

			if (val < threshold)
				return alpha;
		}
	}
	//end of Razoring pruning
	*/
	
	//Futility pruning
	bool f_prune = false;
	if (depth <= 3 && !atCheck && abs(alpha) < 9000){
		static_eval = static_set ? static_eval : Evaluation::evaluate(board, side);
		static_set = true;

		if (static_eval + FUTIL_MARGIN[depth] <= alpha)
			f_prune = true;
	} 
	
	MoveList moves;
	MoveGen::pseudoLegalMoves(&board, side, moves, atCheck);
	orderMoves(board, moves, pvMove);

	int legal = 0;
	int oldAlpha = alpha;
	int bestMove = Move::NO_MOVE;
	score = -INFINITE;
	int bestScore = -INFINITE;
	
	for (int i = 0; i < moves.size(); i++){
		BoardState undo = board.makeMove(moves.get(i));
		if (!undo.valid)
			continue;		

		legal++;
		bool oppAtCheck = MoveGen::isSquareAttacked(&board, oppKingSQ, side);
		int tmp_mv = moves.get(i);
		int mv_from = Move::from(tmp_mv);
		int mv_to = Move::to(tmp_mv);

		//Futility pruning
		if (f_prune && legal > 1 && !Move::captured(tmp_mv) && !Move::promoteTo(tmp_mv) && !oppAtCheck){
			board.undoMove(tmp_mv, undo);
			continue;
		}

		bool doReduce = false;

		/* LMR */
		if (depth > 3 && legal > 3 && (!atCheck) &&
			Move::captured(tmp_mv) == 0 && Move::promoteTo(tmp_mv) == 0  
			&& tmp_mv != board.searchKillers[0][board.ply] && tmp_mv != board.searchKillers[1][board.ply]){ //removed && !oppAtCheck
				int reduce = legal > 6 ? 2 : 1;
				doReduce = true;
				score = -alphaBeta(-beta, -alpha, depth - 1 - reduce, true);
		}
		else {
			score = -alphaBeta(-beta, -alpha, depth - 1, true);
		}

		//re-search
		if (score > alpha && doReduce){
			score = -alphaBeta(-beta, -alpha, depth - 1, true);
		}

		board.undoMove(moves.get(i), undo);

		//check stop
		if (info.stopped){
			return 0;
		}
		
		//if (score > alpha){
		if (score > bestScore){

			bestScore = score;
			bestMove = moves.get(i);

			if (score > alpha) {
				if (score >= beta){
					
					if (legal == 1)
						info.fhf++;

					info.fh++;

					//Killers
					int capt = Move::captured(moves.get(i));
					bool ep = Move::isEP(moves.get(i));

					if (capt == 0 && !ep){
						board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
						board.searchKillers[0][board.ply] = moves.get(i);
					}					
					HashTable::storeHashEntry(board, bestMove, beta, HFBETA, depth);					
					return beta;
				}
				alpha = score;

				//History
				int capt = Move::captured(bestMove);
				bool ep = Move::isEP(bestMove);
				bool isCastle = Move::isCastle(bestMove);

				if (capt == 0 && !(ep || isCastle)){
					int piece = board.board[Move::from(bestMove)];
					board.searchHistory[piece][Move::to(bestMove)] += depth * depth;
				}
			}
		}
	}
	
	//Mate or Stalemate detection
	if (legal == 0){
		if (atCheck){
			return -INFINITE + board.ply;
		} else{
			return 0;
		}
	}
	
	//Hash table
	if (alpha != oldAlpha){
		HashTable::storeHashEntry(board, bestMove, bestScore, HFEXACT, depth);
	} else{		
		HashTable::storeHashEntry(board, bestMove, bestScore, HFALPHA, depth);
	}
	return bestScore;
}

int Search::Quiescence(int alpha, int beta){
	assert(alpha < beta);

	//check time
	if ((info.nodes & 2047) == 0){
		checkUp(info);
	}

	info.nodes++;
	
	if ((board.state.halfMoves >= 100 || board.isRepetition()) && board.ply > 0){		
		return 0;
	}

	int side = board.state.currentPlayer;

	if (board.ply >= Board::MAX_DEPTH - 1){		
		return Evaluation::evaluate(board, side);
	}
	
	int opp = side^1;
	int ks = board.kingSQ[side];
	bool atCheck = MoveGen::isSquareAttacked(&board, ks, opp);

	int score = Evaluation::evaluate(board, side);
	int stand_pat = score;

	if (!atCheck){		
		if (score >= beta){
			return beta;
		}

		if (score > alpha){
			alpha = score;
		}
	}

	MoveList moves;
	
	if (atCheck){		
		U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
		MoveGen::getEvasions(&board, side, moves, occup);
	}
	else {
		MoveGen::pseudoLegalCaptureMoves(&board, side, moves);
		MoveGen::pawnPromotions(&board, side, moves, true);
	}
	
	orderMoves(board, moves, Move::NO_MOVE);

	int legal = 0;
	int oldAlpha = alpha;
	score = -INFINITE;
	
	//U64 pinned = MoveGen::pinnedBB(&board, side, ks);
	
	//Loop through captures
	for (int i = 0; i < moves.size(); i++){

		//Delta cutoff (disable if endgame)
		int capt = Move::captured(moves.get(i));
		int promo = Move::promoteTo(moves.get(i));

		if ((stand_pat +  abs(Evaluation::PIECE_VALUES[capt]) + 200 < alpha) && !atCheck &&			
			(board.material[opp] - Evaluation::KING_VAL - abs(Evaluation::PIECE_VALUES[capt]) > ENDGAME_MAT) && (promo == 0)) {
			continue;
		}
		//Delta cutoff

		//bad captures
		if (!promo && !atCheck && isBadCapture(board, moves.get(i), side)){
			continue;
		}

		BoardState undo = board.makeMove(moves.get(i));
		if (!undo.valid)
			continue;

		legal++;

		score = -Quiescence(-beta, -alpha);
		board.undoMove(moves.get(i), undo);

		//check stop
		if (info.stopped){
			return 0;
		}

		if (score > alpha){
			if (score >= beta){
				
				if (legal == 1)
					info.fhf++;
				info.fh++;
				
				return beta;
			}
			alpha = score;			
		}
	}
	
	if (legal == 0){
		if (atCheck){
			return -MATE_SCORE + board.ply;
		} 
	}
	return alpha;
}

bool Search::isBadCapture(const Board& board, int move, int side){
	int from = Move::from(move);	
	int to = Move::to(move);	
	int attacker = board.board[from];
	int target = board.board[to];

	return Search::see(&board, to, target, from, attacker) < 0;
}

U64 getLeastValuablePiece(const Board* board, U64 attadef, int side, int& piece){
	for (piece = Board::PAWN + side; piece <= Board::KING + side; piece+= 2){
		U64 subset = attadef & board->bitboards[piece];
		if (subset)
			return subset & -subset;
	}
	return 0;
}

#include "Magic.h"
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
    // You can keep your considerXrays function, it is logically correct.

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

