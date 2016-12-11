#include "Search.h"
#include <iostream>
#include "MoveGen.h"
#include "Move.h"
#include "Evaluation.h"
#include <algorithm>
#include "HashTable.h"

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

/*
U64 Search::interval_ms(const time_interv& t0, const time_interv& t1){
	return std::chrono::duration_cast<milliseconds>(t1 - t0).count();
	//return 1000*(t1 - t0)/CLOCKS_PER_SEC;
}*/

U64 Search::getTime(){
	//return high_resolution_clock::now();//.time_since_epoch();
	//return clock();
	U64 t = (U64)(std::chrono::system_clock::now().time_since_epoch()/std::chrono::milliseconds(1));
	return t;
}

void Search::checkUp(SearchInfo& info){
	if (info.timeSet && (Search::getTime() > info.stopTime)){
		info.stopped = true;
	}
}

//MoveScore Search::moveScore[Move::MAX_LEGAL_MOVES];
std::vector<MoveScore> Search::moveScore(Move::MAX_LEGAL_MOVES);

void Search::orderMoves(Board& board, MoveList& moves, int pvMove){

	int side = BoardState::currentPlayer(board.state);

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
				if (moveScore[i].score == 0){
					if (side == Board::WHITE)
						moveScore[i].score = Evaluation::PIECE_SQUARES[piece][to];
					else
						moveScore[i].score = Evaluation::PIECE_SQUARES[piece][Evaluation::MIRROR64[to]];
				}
			}
		}

		//PV override
		if (mv == pvMove)
			moveScore[i].score = PV_BONUS;
	}
	
	std::sort(moveScore.begin(), moveScore.begin() + moves.size(), std::less<MoveScore>());
	for (int i = 0; i < moves.size(); i++)
		moves.set(i, moveScore[i].move);
}

void Search::clearSearch(){
	//TODO USE define at idxs
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

void Search::search(){
	int bestMove = Move::NO_MOVE;
	int bestScore = -INFINITE;
	int pvMoves = 0; //how many pv moves
	
	clearSearch();

	//iterative deepening
	for (int currentDepth = 1; currentDepth <= info.depth; currentDepth++){
		bestScore = alphaBeta(-INFINITE, INFINITE, currentDepth, true);

		//check stop
		if (info.stopped){
			break;
		}

		pvMoves = HashTable::getPVLine(currentDepth, board);
		bestMove = board.pvArray[0];
		
		printf("info score cp %d depth %d nodes %llu time %llu ", 
				bestScore, currentDepth, info.nodes, Search::getTime() - info.startTime);

		printf("pv");

		for (int i = 0; i < pvMoves; i++){
			int m = board.pvArray[i];
			std::cout << " " <<  Move::toLongNotation(m);
		}
		printf("\n");
		/*
		printf("Hits: %d  Overwrite: %d  NewWrite: %d  Cut: %d\nOrdering: %.2f  NullCut:%d\n",
			board.hashTable->hit, board.hashTable->overWrite, board.hashTable->newWrite, board.hashTable->cut,
			(info.fhf/info.fh)*100, info.nullCut);
			*/
	}

	std::cout << "bestmove " << Move::toLongNotation(bestMove) << std::endl;
}

static const int FUTIL_MARGIN[4] = {0, 200, 300, 500};
int Search::alphaBeta(int alpha, int beta, int depth, bool doNull){
	
	/*if (depth == 0){
		return Quiescence(alpha, beta);
	}*/

	//check time
	if ((info.nodes & 2047) == 0){
		checkUp(info);
	}
	
	if ((BoardState::halfMoves(board.state) >= 100 || board.isRepetition()) && board.ply > 0){
		return 0;
	}

	if (board.ply > Board::MAX_DEPTH - 1){
		printf("Reached Max Depth!\n");
		return Evaluation::evaluate(board, BoardState::currentPlayer(board.state));
	}

	//Check extension
	int side = BoardState::currentPlayer(board.state);
	int opp = side^1;
	int kingSQ = numberOfTrailingZeros(board.bitboards[side | Board::KING]);
	int oppKingSQ = numberOfTrailingZeros(board.bitboards[opp | Board::KING]);
	bool atCheck = MoveGen::isSquareAttacked(board, kingSQ, opp);
	
	if (atCheck)
		depth++;

	//Quiesce
	if (depth == 0){
		assert(!atCheck);
		return Quiescence(alpha, beta);
	}

	info.nodes++;
	int score = -INFINITE;
	int pvMove = Move::NO_MOVE;

	if( HashTable::probeHashEntry(board, &pvMove, &score, alpha, beta, depth)){
		board.hashTable->cut++;
		return score;
	}

	/* //eval pruning
	if (depth < 3 && !atCheck && abs(beta - 1) > -INFINITE + 100){
		int static_eval =  Evaluation::evaluate(board, side);
		int eval_margin = 120 * depth;
		if (static_eval - eval_margin >= beta)
			return static_eval - eval_margin;
	}
	*/

	//null move pruning
	bool hasBigPiece = (board.bitboards[side | Board:: ROOK] != 0 || board.bitboards[side | Board:: QUEEN] != 0 ||
		board.bitboards[side | Board:: KNIGHT] != 0 || board.bitboards[side | Board:: BISHOP] != 0);

	if(doNull && !atCheck && board.ply > 0 && hasBigPiece && depth >= 4) {
		int undo_null = board.makeNullMove();
		score = -alphaBeta(-beta, -beta + 1, depth - 4, false);
		board.undoNullMove(undo_null);

		if(info.stopped) 
			return 0;
		
		if (score >= beta && abs(score) < ISMATE){
			info.nullCut++;
		  	return beta;
		}
	}
	//null move pruning

	//Futility pruning
	int f_prune = 0;
	if (depth <= 3 && !atCheck && abs(alpha) < 9000 && 
		Evaluation::evaluate(board, BoardState::currentPlayer(board.state)) + FUTIL_MARGIN[depth] <= alpha)
		f_prune = 1;

	//Move list
	MoveList moves;
	MoveGen::pseudoLegalMoves(board, side, moves, atCheck);
	orderMoves(board, moves, pvMove);

	int legal = 0;
	int oldAlpha = alpha;
	int bestMove = Move::NO_MOVE;
	score = -INFINITE;
	int bestScore = -INFINITE;

	//pinned
	U64 pinned = MoveGen::pinnedBB(board, side, kingSQ);
	
	//Loop through moves
	for (int i = 0; i < moves.size(); i++){

		if (!MoveGen::isLegalMove(board, moves.get(i), side, atCheck, pinned)){
			continue;
		}

		legal++;

		int undo = board.makeMove(moves.get(i));
		bool oppAtCheck = MoveGen::isSquareAttacked(board, oppKingSQ, side);
		int tmp_mv = moves.get(i);
		int mv_from = Move::from(tmp_mv);
		int mv_to = Move::to(tmp_mv);

		//Futility pruning
		if (f_prune && legal > 0 && Move::captured(tmp_mv) == 0 && Move::promoteTo(tmp_mv) == 0 && !oppAtCheck){
			board.undoMove(tmp_mv, undo);
			continue;
		}

		bool doReduce = false;

		/* LMR TODO FIX */
		if (depth > 3 && legal > 3 && (!atCheck) &&
			Move::captured(tmp_mv) == 0 && Move::promoteTo(tmp_mv) == 0 && 
			(mv_from != Move::from(board.searchKillers[0][board.ply]) || mv_to != Move::to(board.searchKillers[0][board.ply])) &&
			(mv_from != Move::from(board.searchKillers[1][board.ply]) || mv_to != Move::to(board.searchKillers[1][board.ply])) &&
			!oppAtCheck){
				int reduce = legal > 6 ? 2 : 1;
				doReduce = true;
				score = -alphaBeta(-beta, -alpha, depth - 1 - reduce, true);
		}
		else {//no LMR
			score = -alphaBeta(-beta, -alpha, depth - 1, true);
		}

		//re-search
		if (score > alpha && doReduce){
			score = -alphaBeta(-beta, -alpha, depth - 1, true);
		}
		/* LMR TODO FIX */

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

					/*
					if (legal == 1)
						info.fhf++;

					info.fh++;
					*/

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
					board.searchHistory[piece][Move::to(bestMove)] += depth * depth; //or only depth
				}
			}
		}
	}
	
	//Mate or Stalemate detection
	//if (moves.size() == 0){BUG WHEN THERE'S AT LEAST ONE ILLEGAL MOVE ie, 5R2/8/8/3BK2p/7k/6p1/8/8 w - - 2 62
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
		HashTable::storeHashEntry(board, bestMove, alpha, HFALPHA, depth);
	}

	return alpha;
}

int Search::Quiescence(int alpha, int beta){

	//check time
	if ((info.nodes & 2047) == 0){
		checkUp(info);
	}

	info.nodes++;
	
	if (BoardState::halfMoves(board.state) >= 100 || board.isRepetition()){
		return 0;
	}
	
	int side = BoardState::currentPlayer(board.state);
	int opp = side^1;

	if (board.ply > Board::MAX_DEPTH - 1){
		printf("Reached Max Depth on Quiescence\n");
		return Evaluation::evaluate(board, side);
	}

	int score = Evaluation::evaluate(board, side);
	int stand_pat = score;

	//standing pat
	if (score >= beta){
		return beta;
	}

	if (score > alpha){
		alpha = score;
	}
	
	//All caps
	MoveList moves;
	MoveGen::pseudoLegalCaptureMoves(board, side, moves);

	//PV move
	//int pvMove = PVTable::probe(board);
	int pvMove = Move::NO_MOVE;

	orderMoves(board, moves, pvMove);

	int legal = 0;
	int oldAlpha = alpha;
	int bestMove = Move::NO_MOVE;
	score = -INFINITE;
	int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);
	bool atCheck = MoveGen::isSquareAttacked(board, ks, opp);
	U64 pinned = MoveGen::pinnedBB(board, side, ks);
	
	//Loop through captures
	for (int i = 0; i < moves.size(); i++){
		if (!MoveGen::isLegalMove(board, moves.get(i), side, atCheck, pinned))
			continue;

		legal++;

		//Delta cutoff (disable if endgame, ie material <= 1300)
		int capt = Move::captured(moves.get(i));
		int promo = Move::promoteTo(moves.get(i));

		if ((stand_pat +  abs(Evaluation::PIECE_VALUES[capt]) + 200 < alpha) &&
			(Evaluation::materialValueSide(board, opp) - abs(Evaluation::PIECE_VALUES[capt]) > 1300) && (promo == 0)){
			continue;
		}
		//Delta cutoff

		int undo = board.makeMove(moves.get(i));
		score = -Quiescence(-beta, -alpha);
		board.undoMove(moves.get(i), undo);

		//check stop
		if (info.stopped){
			return 0;
		}

		if (score > alpha){
			if (score >= beta){
				/*
				if (legal == 1)
					info.fhf++;
				info.fh++;
				*/

				return beta;
			}
			alpha = score;
			bestMove = moves.get(i);
		}
	}
	
	//Pv table
	/*
	if (alpha != oldAlpha){
		PVTable::insert(board, bestMove);
	}*/
	return alpha;
}




