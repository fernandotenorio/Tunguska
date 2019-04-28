#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "Move.h"
#include "Board.h"
#include "defs.h"

class MoveGen{

	public:	
		static int epCaptDiff[2];
		//static MoveList getAllCaptures(Board* board, int side);

		static U64 attackers_to(const Board* board, int sq, int bySide);
		static void solve_check_moves(const Board* board, U64 targets, int checker_sq, int side, MoveList& moves, U64 occup);
		static void getEvasions(const Board* board, int side, MoveList& moves, U64 occup);
		
		static void legalMoves(Board* board, int side, MoveList& valid, bool atCheck);
		static void pseudoLegalMoves(Board* board, int side, MoveList& moves, bool atCheck);
		static bool isLegalMove(Board* board, int move, int side, bool atCheck, U64 pinned);
		static void pseudoLegalCaptureMoves(Board* board, int side, MoveList& capts);

		static bool can_castle_ks(Board* board, int side, U64 occup);
		static bool can_castle_qs(Board* board, int side, U64 occup);
		static void pawnMoves(const Board* board, int side, MoveList& moves, U64 occup);
		static void pawnCaptures(const Board* board, int side, MoveList& moves);
		static void addPromotionsForDir(const Board* board, int side, U64 pushes, int diff[], int flags, MoveList& moves);
		static void addMovesForDir(const Board* board, int side, U64 capts, int diff[], int flags, MoveList& moves);
		static void add_moves(const Board* board, int from, U64 targets, int flags, MoveList& moves);
		static void knightMoves(const Board* board, int side, MoveList& moves);
		static void knightCaptures(const Board* board, int side, MoveList& moves);
		static void kingMoves(const Board* board, int side, MoveList& moves);
		static void kingCaptures(const Board* board, int side, MoveList& moves);
		static void rookMoves(const Board* board, int side, MoveList& moves, U64 occup);
		static void rookCaptures(const Board* board, int side, MoveList& moves, U64 occup);
		static void bishopMoves(const Board* board, int side, MoveList& moves, U64 occup);
		static void bishopCaptures(const Board* board, int side, MoveList& moves, U64 occup);
		static void queenMoves(const Board* board, int side, MoveList& moves, U64 occup);
		static void queenCaptures(const Board* board, int side, MoveList& moves, U64 occup);
		static bool isSquareAttacked(const Board* board, int sq, int bySide);
		static U64 xrayRook(const Board* board, U64 blockers, int from, int side, U64 occup);
		static U64 xrayBishop(const Board* board, U64 blockers, int from, int side, U64 occup);
		static U64 pinnedBB(const Board* board, int side, int kingSQ);
};

#endif
