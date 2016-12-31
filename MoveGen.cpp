#include "MoveGen.h"
#include "Move.h"
#include "BitBoardGen.h"
#include <iostream>

int MoveGen::epCaptDiff[2] = {-8, 8};

static U64 bb_bk[14];
bool MoveGen::isLegalMove(Board& board, int move, int side, bool atCheck, U64 pinned){
	
	if (Move::isCastle(move)){
		assert(!atCheck);
		return true;
	}

	int theKing = side | Board::KING;
	assert(board.bitboards[theKing] != 0);
	int startKingSQ = numberOfTrailingZeros(board.bitboards[theKing]);
	int opp = side^1;

	int from = Move::from(move);
	int to = Move::to(move);
	//capt not set for EP capture
	int capt = Move::captured(move);

	assert((capt != Board::WHITE_KING) || (capt != Board::BLACK_KING));

	int movingPiece = board.board[from];
	bool isEP = Move::isEP(move);
	bool isPinned = (BitBoardGen::SQUARES[from] & pinned) != 0;
	bool kingMoving = (movingPiece - side) == Board::KING;

	//if (!(isEP || isPinned || atCheck || kingMoving)){
	//possible illegal evasions: king moving to check, pinned
	if (!(isEP || isPinned || kingMoving)){
		return true;
	}

	//if (isPinned && !atCheck){
	if (isPinned){
		return (BitBoardGen::LINES_BB[from][to] & BitBoardGen::SQUARES[startKingSQ]) != 0;
	}

	//EP or King moving
	for (int j = 0; j < 14; ++j)
		bb_bk[j] = board.bitboards[j];

	//Update bitboards
	board.bitboards[side] = BitBoardGen::zeroBit(board.bitboards[side], from);
	board.bitboards[side] = BitBoardGen::setBit(board.bitboards[side], to);
	board.bitboards[movingPiece] = BitBoardGen::zeroBit(board.bitboards[movingPiece], from);
	board.bitboards[movingPiece] = BitBoardGen::setBit(board.bitboards[movingPiece], to);
	
	//remove captured ep pawn
	if (isEP){
		board.bitboards[Board::PAWN | opp] = BitBoardGen::zeroBit(board.bitboards[Board::PAWN | opp], to + epCaptDiff[side]);
		board.bitboards[opp] = BitBoardGen::zeroBit(board.bitboards[opp], to + epCaptDiff[side]);
	}
	//capture
	else if (capt){
		assert((capt != Board::WHITE_KING) || (capt != Board::BLACK_KING));
		board.bitboards[capt] = BitBoardGen::zeroBit(board.bitboards[capt], to);
		board.bitboards[opp] = BitBoardGen::zeroBit(board.bitboards[opp], to);		
	}

	//if king is moving, update king square
	assert(board.bitboards[theKing] != 0);
	int kingSQ = movingPiece == theKing ? numberOfTrailingZeros(board.bitboards[theKing]) : startKingSQ;
	//int kingSQ = movingPiece == theKing ? to : startKingSQ;
	bool tmpCheck =	isSquareAttacked(board, kingSQ, opp);
	
	//Undo bitboards
	//for (int j = 0; j < 14; j++)
	//	board.bitboards[j] = bb_bk[j];

	board.bitboards[side] = bb_bk[side];
	board.bitboards[movingPiece] = bb_bk[movingPiece];

	if (capt){
		board.bitboards[capt] = bb_bk[capt];
		board.bitboards[opp] = bb_bk[opp];
	}
	else if (isEP){
		board.bitboards[Board::PAWN | opp] = bb_bk[Board::PAWN | opp];
		board.bitboards[opp] = bb_bk[opp];
	}
	return !tmpCheck;
}


void MoveGen::pseudoLegalCaptureMoves(Board& board, int side, MoveList& capts){
	pawnCaptures(board, side, capts);
	knightCaptures(board, side, capts);
	bishopCaptures(board, side, capts);
	rookCaptures(board, side, capts);
	queenCaptures(board, side, capts);
	kingCaptures(board, side, capts);
}

void MoveGen::pseudoLegalMoves(Board& board, int side, MoveList& moves, bool atCheck){

	if (atCheck){
		getEvasions(board, side, moves);
	}
    else{
		pawnMoves(board, side, moves);
		rookMoves(board, side, moves);
		knightMoves(board, side, moves);
		bishopMoves(board, side, moves);
		queenMoves(board, side, moves);
		kingMoves(board, side, moves);	
	}

	if(!atCheck && can_castle_ks(board, side)){
		int mv = Move::get_move(0, side, 0, 0, 0, 0, Move::CASTLE_FLAG);
		moves.add(mv);
	}
	if (!atCheck && can_castle_qs(board, side)){		
		int mv = Move::get_move(1, side, 0, 0, 0, 0, Move::CASTLE_FLAG);
		moves.add(mv);
	}
}

void MoveGen::legalMoves(Board& board, int side, MoveList& valid, bool atCheck){

	MoveList moves;
	pseudoLegalMoves(board, side, moves, atCheck);
	
	int startKingSQ = numberOfTrailingZeros(board.bitboards[side | Board::KING]);
	U64 pinned = pinnedBB(board, side, startKingSQ);
	
	for (int i = 0; i < moves.size(); i++){
		if (isLegalMove(board, moves.get(i), side, atCheck, pinned)){
			valid.add(moves.get(i));
		}
	}
}
/*
MoveList MoveGen::getAllMoves_bk(Board& board, int side){
	MoveList moves;
	MoveList valid;
	
	pawnMoves(board, side, moves);
	rookMoves(board, side, moves);
	knightMoves(board, side, moves);
	bishopMoves(board, side, moves);
	queenMoves(board, side, moves);
	kingMoves(board, side, moves);	
	
	int theKing = side | Board::KING;
	int startKingSQ = numberOfTrailingZeros(board.bitboards[theKing]);
	int opp = side ^ 1;
	U64 pinned = pinnedBB(board, side, startKingSQ);
	bool atCheck = isSquareAttacked(board, startKingSQ, opp);
	
	//Promotion being checked 4 times
	//Fix: generate only 1 promo move, then add the remaing 3 here
	for (int i = 0; i < moves.size(); i++){
		int move = moves.get(i);
		int from = Move::from(move);
		int to = Move::to(move);
		//cat not set for EP capture
		int capt = Move::captured(move);
		int movingPiece = board.board[from];
		bool isEP = Move::isEP(move);
		bool isPJ = Move::isPJ(move);

		bool isPinned = ((BitBoardGen::ONE << from) & pinned) != 0;
		bool kingMoving = (movingPiece - side) == Board::KING;
			
		if (!(isEP || isPinned || atCheck || kingMoving)){
			valid.add(move);
			continue;
		}
		
		//@Beg Filter illegal
		//Make move
		//board.board[to] = movingPiece;
		//board.board[from] = Board::EMPTY;
		
		//remove captured ep pawn
		if (isEP){
			//board.board[to + epCaptDiff[side]] = Board::EMPTY;
			board.bitboards[Board::PAWN | opp] = BitBoardGen::zeroBit(board.bitboards[Board::PAWN | opp], to + epCaptDiff[side]);
			board.bitboards[opp] = BitBoardGen::zeroBit(board.bitboards[opp], to + epCaptDiff[side]);
		}
		
		//Update bitboards
		board.bitboards[side] = BitBoardGen::zeroBit(board.bitboards[side], from);
		board.bitboards[side] = BitBoardGen::setBit(board.bitboards[side], to);
		board.bitboards[movingPiece] = BitBoardGen::zeroBit(board.bitboards[movingPiece], from);
		board.bitboards[movingPiece] = BitBoardGen::setBit(board.bitboards[movingPiece], to);
		
		//capture
		if (capt != Board::EMPTY){
			board.bitboards[capt] = BitBoardGen::zeroBit(board.bitboards[capt], to);
			board.bitboards[opp] = BitBoardGen::zeroBit(board.bitboards[opp], to);				
		}
		//Update bitboards
		//if king is moving, update king square
		int kingSQ = movingPiece == theKing ? numberOfTrailingZeros(board.bitboards[theKing]) : startKingSQ;
		bool tmpCheck = isSquareAttacked(board, kingSQ, opp);

		//Undo move
		//board.board[from] = movingPiece;
		//board.board[to] = capt;
		
		//undo ep
		if (isEP){
			//board.board[to + epCaptDiff[side]] = Board::PAWN | opp; //had bug here (... | side)
			board.bitboards[Board::PAWN | opp] = BitBoardGen::setBit(board.bitboards[Board::PAWN | opp], to + epCaptDiff[side]);
			board.bitboards[opp] = BitBoardGen::setBit(board.bitboards[opp], to + epCaptDiff[side]);
		}
		
		//undo bitboards
		board.bitboards[side] = BitBoardGen::setBit(board.bitboards[side], from);
		board.bitboards[side] = BitBoardGen::zeroBit(board.bitboards[side], to);
		board.bitboards[movingPiece] = BitBoardGen::setBit(board.bitboards[movingPiece], from);
		board.bitboards[movingPiece] = BitBoardGen::zeroBit(board.bitboards[movingPiece], to);
		
		//undo capture
		if (capt != Board::EMPTY){
			board.bitboards[capt] = BitBoardGen::setBit(board.bitboards[capt], to);
			board.bitboards[opp] = BitBoardGen::setBit(board.bitboards[opp], to);
		}
		//undo bitboards
		
		if (tmpCheck) 
			continue;
		valid.add(move);
		//@End Filter illegal
	}

	if(! atCheck && can_castle_ks(board, side)){
		//atCheck = isSquareAttacked(board, startKingSQ, opp);
		int mv = Move::get_move(0, side, 0, 0, 0, 0, Move::CASTLE_FLAG);
		valid.add(mv);
	}
	if (! atCheck && can_castle_qs(board, side)){
		int mv = Move::get_move(1, side, 0, 0, 0, 0, Move::CASTLE_FLAG);
		valid.add(mv);
	}
	return valid;
}
*/

bool MoveGen::can_castle_ks(Board& board, int side){
	U64 occup = board.bitboards[side] | board.bitboards[side^1];
	
	if (!BoardState::can_castle_ks(board.state, side))
		return false;
	if ((occup & BitBoardGen::KS_CASTLE_OCCUP[side]) != 0)
		return false;
	
	//@optimize
	int opp = side ^ 1;
	if (isSquareAttacked(board, Board::KS_CASTLE_ATTACK[side][0], opp) || 
		isSquareAttacked(board, Board::KS_CASTLE_ATTACK[side][1], opp))
		return false;
	
	return true;
}

bool MoveGen::can_castle_qs(Board& board, int side){
	U64 occup = board.bitboards[side] | board.bitboards[side^1];
	
	if (!BoardState::can_castle_qs(board.state, side))
		return false;
	if ((occup & BitBoardGen::QS_CASTLE_OCCUP[side]) != 0)
		return false;
	
	//@optimize
	int opp = side ^ 1;
	if (isSquareAttacked(board, Board::QS_CASTLE_ATTACK[side][0], opp) || 
		isSquareAttacked(board, Board::QS_CASTLE_ATTACK[side][1], opp))
		return false;
	
	return true;
}

static int dirs[2][2] = {{7, 64 - 9}, {9, 64 - 7}};
static int diffs[2][2] = {{7, -9}, {9, -7}};
static int promo_ranks[2] = {7, 0};
static int push_dir[2] = {8, 64 - 8};
static int push_ranks[2] = {2, 5};
static int diff_push[2] = {8, -8};
static int diff_jump[2] = {16, -16};

void MoveGen::pawnCaptures(const Board& board, int side, MoveList& moves){

	U64 pawnBB = board.bitboards[Board::PAWN | side];
	int opp = side^1;		
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];
	int epSquare = BoardState::epSquare(board.state);
	
	for (int i = 0; i < 2; i++){
		int *dir = dirs[i];
		int *diff = diffs[i];
		U64 wFile = BitBoardGen::WRAP_FILES[i];
		
		//captures left + right
		U64 attacks = BitBoardGen::circular_lsh(pawnBB, dir[side]) & ~wFile;	
		U64 capts = attacks & enemy;
		U64 promotions_capt = capts & BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
		//remove promotions from captures
		capts &= ~BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
		
		//en passant captures
		U64 epBB = epSquare != 0 ? BitBoardGen::SQUARES[epSquare] : 0;
		U64 eppCapt = attacks & epBB;
		
		addMovesForDir(board, side, capts, diff, Move::NO_FLAGS, moves);
		addMovesForDir(board, side, eppCapt, diff, Move::EP_FLAG, moves);
		addPromotionsForDir(board, side, promotions_capt, diff, Move::NO_FLAGS, moves);
	}
}

void MoveGen::pawnMoves(const Board& board, int side, MoveList& moves){

	pawnCaptures(board, side, moves);

	U64 pawnBB = board.bitboards[Board::PAWN | side];
	int opp = side^1;
	
	//pushes
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 pushes = BitBoardGen::circular_lsh(pawnBB, push_dir[side]) & ~occup;	
	
	U64 dpushes = BitBoardGen::circular_lsh((pushes & BitBoardGen::BITBOARD_RANKS[push_ranks[side]]), push_dir[side]);
	dpushes &= ~occup;
	
	U64 promotions_quiet = pushes & BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
	//remove promotion from pushes
	pushes &= ~BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
	
	addMovesForDir(board, side, pushes, diff_push, Move::NO_FLAGS, moves);
	addMovesForDir(board, side, dpushes, diff_jump, Move::PAWN_JUMP_FLAG, moves);
	addPromotionsForDir(board, side, promotions_quiet, diff_push, Move::NO_FLAGS, moves);
}

void MoveGen::addPromotionsForDir(const Board& board, int side, U64 pushes, int diff[], int flags, MoveList& moves){
	while (pushes){
		int to = numberOfTrailingZeros(pushes);
		int from = to - diff[side];
		int mov1 = Move::get_move(from, to, board.board[to], Board::QUEEN | side, 0, 0, 0) | flags;
		int mov2 = Move::get_move(from, to, board.board[to], Board::ROOK | side, 0, 0, 0) | flags;
		int mov3 = Move::get_move(from, to, board.board[to], Board::KNIGHT | side, 0, 0, 0) | flags;
		int mov4 = Move::get_move(from, to, board.board[to], Board::BISHOP | side, 0, 0, 0) | flags;
		moves.add(mov1); moves.add(mov2); moves.add(mov3); moves.add(mov4);
		pushes &= pushes - 1;
	}
}

void MoveGen::addMovesForDir(const Board& board, int side, U64 capts, int diff[], int flags, MoveList& moves){
	while (capts){
		int to = numberOfTrailingZeros(capts);
		int from = to - diff[side];
		int move = Move::get_move(from, to, board.board[to], 0, 0, 0, 0) | flags;
		moves.add(move);
		capts &= capts - 1;
	}
}

void MoveGen::add_moves(const Board& board, int from, U64 targets, int flags, MoveList& moves){
	while (targets){
		int to = numberOfTrailingZeros(targets);
		int move = Move::get_move(from, to, board.board[to], 0, 0, 0, 0) | flags;
		moves.add(move);
		targets &= targets - 1;
	}
}

void MoveGen::knightCaptures(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 kn = board.bitboards[Board::KNIGHT | side];
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];

	while (kn){
		int from = numberOfTrailingZeros(kn);
		U64 targets = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from] & enemy;
		add_moves(board, from, targets, Move::NO_FLAGS, moves);
		kn &= kn - 1;
	}
}

void MoveGen::knightMoves(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 kn = board.bitboards[Board::KNIGHT | side];
	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];

	while (kn){
		int from = numberOfTrailingZeros(kn);
		U64 targets = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from] & enemyOrEmpty;
		add_moves(board, from, targets, Move::NO_FLAGS, moves);
		kn &= kn - 1;
	}
}

void MoveGen::kingCaptures(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 king = board.bitboards[Board::KING | side];
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];

	while (king){
		int from = numberOfTrailingZeros(king);
		U64 targets = BitBoardGen::BITBOARD_KING_ATTACKS[from] & enemy;
		add_moves(board, from, targets, Move::NO_FLAGS, moves);
		king &= king - 1;
	}
}

void MoveGen::kingMoves(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 king = board.bitboards[Board::KING | side];
	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];

	while (king){
		int from = numberOfTrailingZeros(king);
		U64 targets = BitBoardGen::BITBOARD_KING_ATTACKS[from] & enemyOrEmpty;
		add_moves(board, from, targets, Move::NO_FLAGS, moves);
		king &= king - 1;
	}
}

U64 MoveGen::upwardMoveTargetsFrom(int from, U64 occup, U64 dir_mask[], U64 enemyOrEmpty){
	U64 ray = dir_mask[from];
	U64 blocker = occup & ray;
	
	if (blocker){
		int sq = numberOfTrailingZeros(blocker);
		ray^= dir_mask[sq];
	} 
	return ray & enemyOrEmpty;
}

U64 MoveGen::downwardMoveTargetsFrom(int from, U64 occup, U64 dir_mask[], U64 enemyOrEmpty){
	U64 ray = dir_mask[from];
	U64 blocker = occup & ray;
	
	if (blocker){
		int sq = numberOfLeadingZeros(blocker);
		ray^= dir_mask[sq];
	} 
	return ray & enemyOrEmpty;
}

void MoveGen::rookCaptures(const Board& board, int side, MoveList& moves){
	int opp = side ^ 1;
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];
	U64 rooks = board.bitboards[Board::ROOK | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];

	while (rooks){	
		int from = numberOfTrailingZeros(rooks);
		U64 rookAtt = rookAttacks(board, occup, from, side);
		add_moves(board, from, rookAtt & enemy, Move::NO_FLAGS, moves);
		rooks &= rooks - 1;
	}
}

void MoveGen::rookMoves(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 rooks = board.bitboards[Board::ROOK | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	
	U64 *up = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP];
	U64 *right = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_RIGHT];
	U64 *down = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN];
	U64 *left = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_LEFT];
	
	while (rooks){	
		int from = numberOfTrailingZeros(rooks);
		U64 upward =  upwardMoveTargetsFrom(from, occup, up, enemyOrEmpty) | upwardMoveTargetsFrom(from, occup, right, enemyOrEmpty);
		U64 downward =  downwardMoveTargetsFrom(from, occup, down, enemyOrEmpty) | downwardMoveTargetsFrom(from, occup, left, enemyOrEmpty);
		add_moves(board, from, upward | downward, Move::NO_FLAGS, moves);
		rooks &= rooks - 1;
	}
}

void MoveGen::bishopCaptures(const Board& board, int side, MoveList& moves){
	int opp = side ^ 1;
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];
	U64 bishops = board.bitboards[Board::BISHOP | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];

	while (bishops){	
		U64 capts = 0;
		int from = numberOfTrailingZeros(bishops);
		U64 bishopAtt = bishopAttacks(board, occup, from, side);
		add_moves(board, from, bishopAtt & enemy, Move::NO_FLAGS, moves);
		bishops &= bishops - 1;
	}
}

void MoveGen::bishopMoves(const Board& board, int side, MoveList& moves){
	int opp = side ^ 1;
	U64 bishops = board.bitboards[Board::BISHOP | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	
	U64 *up_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_RIGHT];
	U64 *up_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_LEFT];
	U64 *down_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_RIGHT];
	U64 *down_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_LEFT];
	
	while (bishops){
		int from = numberOfTrailingZeros(bishops);
		U64 upward =  upwardMoveTargetsFrom(from, occup, up_r, enemyOrEmpty) | upwardMoveTargetsFrom(from, occup, up_l, enemyOrEmpty);
		U64 downward =  downwardMoveTargetsFrom(from, occup, down_r, enemyOrEmpty) | downwardMoveTargetsFrom(from, occup, down_l, enemyOrEmpty);
		add_moves(board, from, upward | downward, Move::NO_FLAGS, moves);
		bishops &= bishops - 1;
	}
}

void MoveGen::queenCaptures(const Board& board, int side, MoveList& moves){
	int opp = side^1;
	U64 enemy = board.bitboards[opp] & ~board.bitboards[Board::KING | opp];
	U64 queens = board.bitboards[Board::QUEEN | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];

	while (queens){	
		int from = numberOfTrailingZeros(queens);
		U64 rookAtt = rookAttacks(board, occup, from, side);
		U64 bishopAtt = bishopAttacks(board, occup, from, side);
		add_moves(board, from, (rookAtt | bishopAtt) & enemy, Move::NO_FLAGS, moves);
		queens &= queens - 1;
	}
}

void MoveGen::queenMoves(const Board& board, int side, MoveList& moves){
	int opp = side ^ 1;
	U64 queens = board.bitboards[Board::QUEEN | side];
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	
	U64 *up = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP];
	U64 *up_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_LEFT];
	U64 *up_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_RIGHT];
	U64 *right = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_RIGHT];
	
	U64 *down = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN];
	U64 *down_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_LEFT];
	U64 *down_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_RIGHT];
	U64 *left = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_LEFT];
	
	while (queens){	
		int from = numberOfTrailingZeros(queens);
		U64 upward =  upwardMoveTargetsFrom(from, occup, up_r, enemyOrEmpty) | upwardMoveTargetsFrom(from, occup, up_l, enemyOrEmpty)
							  | upwardMoveTargetsFrom(from, occup, up, enemyOrEmpty) | upwardMoveTargetsFrom(from, occup, right, enemyOrEmpty);
		U64 downward =  downwardMoveTargetsFrom(from, occup, down_r, enemyOrEmpty) | downwardMoveTargetsFrom(from, occup, down_l, enemyOrEmpty)
							 | downwardMoveTargetsFrom(from, occup, down, enemyOrEmpty) | downwardMoveTargetsFrom(from, occup, left, enemyOrEmpty);
		add_moves(board, from, upward | downward, Move::NO_FLAGS, moves);
		queens &= queens - 1;
	}
}
/*
bool MoveGen::isSquareAttacked(const Board& board, int sq, int bySide){
	int opp = bySide ^ 1;
	U64 pawns = board.bitboards[Board::PAWN | bySide];

	if ((BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][sq] & pawns) != 0)
		return true;
	
	U64 knights = board.bitboards[Board::KNIGHT | bySide];
	if ((BitBoardGen::BITBOARD_KNIGHT_ATTACKS[sq] & knights) != 0)
		return true;
	
	U64 king = board.bitboards[Board::KING | bySide];
	if ((BitBoardGen::BITBOARD_KING_ATTACKS[sq] & king) != 0)
		return true;
	
	//Bishop Queen
	//enemyOrEmpty is the attacking side (us or empty)
	U64 enemyOrEmpty = ~board.bitboards[opp];//& ~board.bitboards[Board.KING | opp];
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	U64 bishopQueen = board.bitboards[Board::BISHOP | bySide] | board.bitboards[Board::QUEEN | bySide];

	if (bishopQueen){
		U64 *up_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_LEFT];
		U64 *up_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_RIGHT];
		U64 *down_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_LEFT];
		U64 *down_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_RIGHT];
		
		U64 upAttack = upwardMoveTargetsFrom(sq, occup, up_l, enemyOrEmpty) | upwardMoveTargetsFrom(sq, occup, up_r, enemyOrEmpty);
		if ((upAttack & bishopQueen) != 0)
			return true;
		
		U64 downAttack = downwardMoveTargetsFrom(sq, occup, down_l, enemyOrEmpty) | downwardMoveTargetsFrom(sq, occup, down_r, enemyOrEmpty);
		if ((downAttack & bishopQueen) != 0)
			return true;
	}
	
	//Rook Queen
	U64 rookQueen = board.bitboards[Board::ROOK | bySide] | board.bitboards[Board::QUEEN | bySide];
	if (rookQueen){
		U64 *up = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP];
		U64 *right = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_RIGHT];
		U64 *down = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN];
		U64 *left = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_LEFT];
		
		U64 upAttack = upwardMoveTargetsFrom(sq, occup, up, enemyOrEmpty) | upwardMoveTargetsFrom(sq, occup, right, enemyOrEmpty);
		if ((upAttack & rookQueen) != 0)
			return true;
		 
		U64 downAttack = downwardMoveTargetsFrom(sq, occup, down, enemyOrEmpty) | downwardMoveTargetsFrom(sq, occup, left, enemyOrEmpty);
		if ((downAttack & rookQueen) != 0)
			return true;
	}
	
	return false;
}
*/

bool MoveGen::isSquareAttacked(const Board& board, int sq, int bySide){
	int opp = bySide^1;

	//Pawn
	U64 pawns = board.bitboards[Board::PAWN | bySide];
	if (BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][sq] & pawns)
		return true;

	//Knight
	U64 knights = board.bitboards[Board::KNIGHT | bySide];
	if (BitBoardGen::BITBOARD_KNIGHT_ATTACKS[sq] & knights)
		return true;

	//King
	U64 king = board.bitboards[Board::KING | bySide];
	if (BitBoardGen::BITBOARD_KING_ATTACKS[sq] & king)
		return true;

	//Bishop/Queen
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	U64 bishopQueen = board.bitboards[Board::BISHOP | bySide] | board.bitboards[Board::QUEEN | bySide];
	if (bishopQueen){
		if (bishopAttacks(board, occup, sq, 0) & bishopQueen)
			return true;
	} 

	//Rook/Queen
	U64 rookQueen = board.bitboards[Board::ROOK | bySide] | board.bitboards[Board::QUEEN | bySide];
	if (rookQueen){
		if (rookAttacks(board, occup, sq, 0) & rookQueen)
			return true;
	}
	return false;
}

U64 MoveGen::downwardAttackTargetsFrom(int from, U64 occup, U64 dir_mask[]){
	U64 ray = dir_mask[from];
	U64 blocker = occup & ray;
	
	if (blocker != 0){
		int sq = numberOfLeadingZeros(blocker);
		ray^= dir_mask[sq];
	} 
	return ray;
}

U64 MoveGen::upwardAttackTargetsFrom(int from, U64 occup, U64 dir_mask[]){
	U64 ray = dir_mask[from];
	U64 blocker = occup & ray;
	
	if (blocker != 0){
		int sq = numberOfTrailingZeros(blocker);
		ray^= dir_mask[sq];
	} 
	return ray;
}

U64 MoveGen::rookAttacks(const Board& board, U64 occup, int from, int side){
	//int opp = side ^ 1;
	//U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	U64 *up = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP];
	U64 *right = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_RIGHT];
	U64 *down = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN];
	U64 *left = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_LEFT];

	U64 upward =  upwardAttackTargetsFrom(from, occup, up) | upwardAttackTargetsFrom(from, occup, right);
	U64 downward =  downwardAttackTargetsFrom(from, occup, down) | downwardAttackTargetsFrom(from, occup, left);
	return upward | downward;
}

U64 MoveGen::bishopAttacks(const Board& board, U64 occup, int from, int side){
	//int opp = side ^ 1;
	//U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	U64 *up_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_RIGHT];
	U64 *up_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_LEFT];
	U64 *down_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_RIGHT];
	U64 *down_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_LEFT];
	
	U64 upward =  upwardAttackTargetsFrom(from, occup, up_r) | upwardAttackTargetsFrom(from, occup, up_l);
	U64 downward =  downwardAttackTargetsFrom(from, occup, down_r) | downwardAttackTargetsFrom(from, occup, down_l);
	return upward | downward;
}

U64 MoveGen::xrayRook(const Board& board, U64 blockers, int from, int side){
	int opp = side ^ 1;
    U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 attacks = rookAttacks(board, occup, from, side);
	blockers &= attacks;
	return attacks ^ rookAttacks(board, occup ^ blockers, from, side);
}

U64 MoveGen::xrayBishop(const Board& board, U64 blockers, int from, int side){
	int opp = side ^ 1;
    U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 attacks = bishopAttacks(board, occup, from, side);
	blockers &= attacks;
	return attacks ^ bishopAttacks(board, occup ^ blockers, from, side);
}

U64 MoveGen::pinnedBB(const Board& board, int side, int kingSQ){
	int opp =  side^1;
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 pinned = 0;
	U64 pinner = xrayRook(board, board.bitboards[side], 
				kingSQ, opp) & (board.bitboards[Board::ROOK | opp] | board.bitboards[Board::QUEEN | opp]);

	while (pinner) {
	   int sq  = numberOfTrailingZeros(pinner);
	   pinned |= BitBoardGen::RECT_LOOKUP[sq][kingSQ] & board.bitboards[side];
	   pinner&= pinner - 1;
	}
	pinner = xrayBishop(board, board.bitboards[side], 
		kingSQ, opp) & (board.bitboards[Board::BISHOP | opp] | board.bitboards[Board::QUEEN | opp]);

	while (pinner) {
	   int sq  = numberOfTrailingZeros(pinner);
	   pinned|= BitBoardGen::RECT_LOOKUP[sq][kingSQ] & board.bitboards[side];
	   pinner&= pinner - 1;
	}
	return pinned;
}

//Test
U64 MoveGen::attackers_to(const Board& board, int sq, int bySide){
	int opp = bySide^1;
	U64 attackers = 0;
	
	//Pawn
	U64 pawns = board.bitboards[Board::PAWN | bySide];
	attackers|= BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][sq] & pawns;

	//Knight
	U64 knights = board.bitboards[Board::KNIGHT | bySide];
	attackers|= BitBoardGen::BITBOARD_KNIGHT_ATTACKS[sq] & knights;
	
	//King
	U64 king = board.bitboards[Board::KING | bySide];
	attackers|= BitBoardGen::BITBOARD_KING_ATTACKS[sq] & king;

	//Bishop/Queen
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	U64 bishopQueen = board.bitboards[Board::BISHOP | bySide] | board.bitboards[Board::QUEEN | bySide];
	attackers|= bishopAttacks(board, occup, sq, 0) & bishopQueen;

	//Rook/Queen
	U64 rookQueen = board.bitboards[Board::ROOK | bySide] | board.bitboards[Board::QUEEN | bySide];
	attackers|= rookAttacks(board, occup, sq, 0) & rookQueen;
	return attackers;
}

//We are in check (king evasion handled somewhere)
void MoveGen::solve_check_moves(const Board& board, U64 targets, int checker_sq, int side, MoveList& moves){
	
	//knights
	U64 kn = board.bitboards[Board::KNIGHT | side];
	while (kn){
		int from = numberOfTrailingZeros(kn);
		U64 kn_targets = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from] & targets;
		add_moves(board, from, kn_targets, Move::NO_FLAGS, moves);
		kn&= kn - 1;
	}
	
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	//bishops
	U64 bishops = board.bitboards[Board::BISHOP | side];
	while (bishops){	
		int from = numberOfTrailingZeros(bishops);
		U64 bishop_targets = bishopAttacks(board, occup, from, side) & targets;
		add_moves(board, from, bishop_targets, Move::NO_FLAGS, moves);
		bishops&= bishops - 1;
	}
	
	//rooks
	U64 rooks = board.bitboards[Board::ROOK | side];
	while (rooks){	
		int from = numberOfTrailingZeros(rooks);
		U64 rook_targets = rookAttacks(board, occup, from, side) & targets;
		add_moves(board, from, rook_targets, Move::NO_FLAGS, moves);
		rooks&= rooks - 1;
	}
	
	//queens
	U64 queens = board.bitboards[Board::QUEEN | side];
	while (queens){	
		int from = numberOfTrailingZeros(queens);
		U64 rookAtt = rookAttacks(board, occup, from, side);
		U64 bishopAtt = bishopAttacks(board, occup, from, side);
		add_moves(board, from, (rookAtt | bishopAtt) & targets, Move::NO_FLAGS, moves);
		queens&= queens - 1;
	}
	
	//pawn capts (capture the checker)
	int opp = side^1;		
	U64 pawnBB = board.bitboards[Board::PAWN | side];
	int epSquare = BoardState::epSquare(board.state);

	//normal capture
	U64 pawns = board.bitboards[Board::PAWN | side] & BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][checker_sq];

	//if (pawns && (checker_sq != promo_ranks[side])){ DEAD BUG WAS HERE!
	//if (pawns && (checker_sq >> 3) != promo_ranks[side]){ //OK
	if (pawns && !(BitBoardGen::SQUARES[checker_sq] & BitBoardGen::BITBOARD_RANKS[promo_ranks[side]])){
		while(pawns){
			int from = numberOfTrailingZeros(pawns);
			int move = Move::get_move(from, checker_sq, board.board[checker_sq], 0, 0, 0, 0);
			moves.add(move);
			pawns&= pawns - 1;
		}
	}else{ //promotion captures
		while(pawns){
			int from = numberOfTrailingZeros(pawns);
			int mov1 = Move::get_move(from, checker_sq, board.board[checker_sq], Board::QUEEN | side, 0, 0, 0);
			int mov2 = Move::get_move(from, checker_sq, board.board[checker_sq], Board::ROOK | side, 0, 0, 0);
			int mov3 = Move::get_move(from, checker_sq, board.board[checker_sq], Board::KNIGHT | side, 0, 0, 0);
			int mov4 = Move::get_move(from, checker_sq, board.board[checker_sq], Board::BISHOP | side, 0, 0, 0);
			moves.add(mov1); moves.add(mov2); moves.add(mov3); moves.add(mov4);
			pawns&= pawns - 1;
		}
	}
	
	//ep capture
	if (epSquare && (checker_sq == (epSquare + epCaptDiff[side]))) {
		pawns = board.bitboards[Board::PAWN | side] & BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][epSquare];
		while(pawns){
			int from = numberOfTrailingZeros(pawns);
			int move = Move::get_move(from, epSquare, 0, 0, Move::EP_FLAG, 0, 0);
			moves.add(move);
			pawns&= pawns - 1;
		}
	}
	
	//pawns pushes
	U64 pushesNormal = BitBoardGen::circular_lsh(pawnBB, push_dir[side]) & ~occup;
	U64 pushesSolve = pushesNormal & targets;
	U64 dpushes = BitBoardGen::circular_lsh(pushesNormal & BitBoardGen::BITBOARD_RANKS[push_ranks[side]],
		push_dir[side]) & ~occup & targets;
	
	U64 promotions_quiet = pushesSolve & BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
	pushesSolve&= ~BitBoardGen::BITBOARD_RANKS[promo_ranks[side]];
	addMovesForDir(board, side, pushesSolve, diff_push, Move::NO_FLAGS, moves);
	addMovesForDir(board, side, dpushes, diff_jump, Move::PAWN_JUMP_FLAG, moves);
	addPromotionsForDir(board, side, promotions_quiet, diff_push, Move::NO_FLAGS, moves);
	
}

void MoveGen::getEvasions(const Board& board, int side, MoveList& moves){
	
	int opp = side^1;
	int ksq = numberOfTrailingZeros(board.bitboards[side | Board::KING]);
	U64 sliderAttacks = 0;
	U64 checkers = attackers_to(board, ksq, opp);
	U64 sliders = checkers & ~(board.bitboards[Board::KNIGHT | opp] | board.bitboards[Board::PAWN | opp]);
	
	while(sliders){
		int checksq = numberOfTrailingZeros(sliders);
		sliderAttacks|= BitBoardGen::LINES_BB[checksq][ksq] ^ BitBoardGen::SQUARES[checksq];
		sliders&= sliders - 1;
	}

	//King pseudo legal moves
	U64 king_scapes = BitBoardGen::BITBOARD_KING_ATTACKS[ksq] & ~(board.bitboards[side] | sliderAttacks | board.bitboards[Board::KING | opp]);
	add_moves(board, ksq, king_scapes, Move::NO_FLAGS, moves);
	
	///double check, only king can move
	if (checkers & (checkers - 1)){
		return;
	}
	//Generate blocking evasions or captures of the lone checking piece
	int checker_sq = numberOfTrailingZeros(checkers);
	U64 targets = BitBoardGen::RECT_LOOKUP[checker_sq][ksq] | BitBoardGen::SQUARES[checker_sq];
	
	//get moves from other pieces blocking or capturing the checker
	solve_check_moves(board, targets, checker_sq, side, moves);
	//pawnMoves(board, side, moves);
	//rookMoves(board, side, moves);
	//knightMoves(board, side, moves);
	//bishopMoves(board, side, moves);
	//queenMoves(board, side, moves);
}


