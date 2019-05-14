#include "Evaluation.h"
#include "MoveGen.h"
#include "FenParser.h"
#include "Zobrist.h"
#include "Magic.h"
#include <iostream>
#include <fstream>

#define PAWN_SQ {0, 0, 0, 0,  0,  0,  0,  0, 5, 10, 10,-20,-20, 10, 10,  5, 5, -5,-10,  0,  0,-10, -5,  5, 0,  0,  0, 20, 20,  0,  0,  0, 5,  5, 10, 25, 25, 10,  5,  5, 10, 10, 20, 30, 30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50, 0,  0,  0,  0,  0,  0,  0,  0}						
#define KNIGHT_SQ {-50,-40,-30,-30,-30,-30,-40,-50,-40, -20,  0,  5,  5,  0, -20, -40,-30,  5, 10, 15, 15, 10,  5, -30, -30,  0, 15, 20, 20, 15,  0,-30,-30,  5, 15, 20, 20, 15,  5,-30,-30,  0, 10, 15, 15, 10,  0, -30, -40, -20,  0,  0,  0,  0,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50}							
#define BISHOP_SQ {-20,-10,-10,-10,-10,-15,-10,-20,-10,  5,  0,  0,  0,  0,  5,-10,-10, 10, 10, 10, 10, 10, 10,-10,-10,  0, 10, 10, 10, 10,  0,-10,-10,  5,  5, 10, 10,  5,  5,-10,-10,  0,  5, 10, 10,  5,  0,-10,-10,  0,  0,  0,  0,  0,  0,-10,-20,-10,-10,-10,-10,-10,-10,-20}							  
#define ROOK_SQ {-5, 0, 5, 10, 10, 5, 0, -5, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0,  0, 20, 20, 20, 20, 20, 20, 20, 20, 0,  0,  0,  0,  0,  0,  0,  0}
#define QUEEN_SQ {-20,-10,-10, -5, -5,-10,-10,-20,-10,  0,  5,  0,  0,  0,  0,-10,-10,  5,  5,  5,  5,  5,  0,-10,-5,   0,  5,  5,  5,  5,  0, -5,-5,   0,  5,  5,  5,  5,  0, -5,-10,  0,  5,  5,  5,  5,  0,-10,-10,  0,  0,  0,  0,  0,  0,-10,-20,-10,-10, -5, -5,-10,-10,-20}
#define KING_SQ {20, 30, 10,  0,  0, 10, 30, 20,20, 20,  0,  0,  0,  0, 20, 20,-10,-20,-20,-20,-20,-20,-20,-10,-20,-30,-30,-40,-40,-30,-30,-20,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30}							 
#define KING_END_SQ {-50, -40, 0, 0, 0,	0, -40,	-50, -20, 0, 10, 15, 15, 10, 0,	-20, 0,	10,	20,	20,	20,	20,	10,	0, 0, 10, 30, 40, 40, 30, 10, 0, 0,	10,	20,	40,	40,	20,	10,	0, 0, 10, 20, 20, 20, 20, 10, 0, -10, 0, 10, 10, 10, 10, 0,	-10, -50, -10,	0,	0,	0,	0, -10, -50}

//idx 0/1 => light/dark squares bishop
static const int KNB_K_MATE[2][64]= {
		{-5,-5,-10,-10,-20,-30,-30,-50,
		  0,0,-5,-5,-10,-20,-30,-30,
		  5,5,0,0,-5,-10,-10,-20,
		  10,10,5,5,0,-5,-5,-10,
		  -10,-5,-5,0,5,5,10,10,
		  -20,-10,-10,-5,0,0,5,5,
		  -30,-30,-20,-10,-5,-5,0,0,
		 -50,-30,-30,-20,-10,-10,-5,-5},

		{-50,-30,-30,-20,-10,-10,-5,-5,
		-30,-30,-20,-10,-5,-5,0,0,
		-20,-10,-10,-5,0,0,5,5,
		-10,-5,-5,0,5,5,10,10,
		10,10,5,5,0,-5,-5,-10,
		5,5,0,0,-5,-10,-10,-20,
		0,0,-5,-5,-10,-20,-30,-30,
		-5,-5,-10,-10,-20,-30,-30,-50}
};

static const int STORM_MAP[64] = {0,  0,  0,  0,  0,  0,  0,  0,
								  0,  0,  0,  0,  0,  0,  0,  0,
								  0,  0,  0,  0,  0,  0,  0,  0,
								 10, 10, 20,  0,  0, 20, 10, 10,
								 15, 20, 50, 15, 15, 50, 20, 15,
								 30, 50, 60, 50, 50, 60, 50, 30,
								 0,  0,  0,  0,  0,  0,  0,  0,
								 0,  0,  0,  0,  0,  0,  0,  0};

int Evaluation::PIECE_VALUES[14] = {0, 0, PAWN_VAL, -PAWN_VAL, KNIGHT_VAL, -KNIGHT_VAL, BISHOP_VAL,
					-BISHOP_VAL, ROOK_VAL, -ROOK_VAL, QUEEN_VAL, -QUEEN_VAL, KING_VAL, -KING_VAL};
					
int Evaluation::PIECE_SQUARES_MG[14][64] = {{}, {}, 
										PAWN_SQ, PAWN_SQ,
									 	KNIGHT_SQ, KNIGHT_SQ,
									 	BISHOP_SQ, BISHOP_SQ,
										ROOK_SQ, ROOK_SQ,
										QUEEN_SQ, QUEEN_SQ,
										KING_SQ, KING_SQ};

int Evaluation::PIECE_SQUARES_END[14][64] = {{}, {},
											PAWN_SQ, PAWN_SQ,
											KNIGHT_SQ, KNIGHT_SQ,
											BISHOP_SQ, BISHOP_SQ,
											ROOK_SQ, ROOK_SQ,
										 	QUEEN_SQ, QUEEN_SQ,
										  	KING_END_SQ, KING_END_SQ};	
//Bishops
static const int BISHOP_PAIR_MG = 50;
static const int BISHOP_PAIR_EG = 68;
static const int BISHOP_PAWN_PENALTY_MG = -3;
static const int BISHOP_PAWN_PENALTY_EG = -7;

//Open and semiopen files
static const int D_ROOK_7_MG = 40;
static const int D_ROOK_7_EG = 80;
static const int D_ROOK_7_Q = 20;

static const int R_OPEN_MG = 14;
static const int R_SOPEN_MG = 7;
static const int Q_OPEN_MG = 6;
static const int Q_SOPEN_MG = 3;
static const int R_OPEN_EG = 20;
static const int R_SOPEN_EG = 10;
static const int Q_OPEN_EG = 8;
static const int Q_SOPEN_EG = 4;
static const int OPENFILES_BONUS_MG[2][2] = {{R_OPEN_MG, R_SOPEN_MG},{Q_OPEN_MG, Q_SOPEN_MG}}; 
static const int OPENFILES_BONUS_EG[2][2] = {{R_OPEN_EG, R_SOPEN_EG},{Q_OPEN_EG, Q_SOPEN_EG}};

//Pawns
static const int ISOLATED_PAWN_PENALTY_MG[8] = {-5, -7, -10, -10, -10, -10, -7, -5};
static const int ISOLATED_PAWN_PENALTY_EG[8] = {-10, -14, -20, -20, -20, -20, -14, -10};
static const int PASSED_PAWN_BONUS_MG[2][8] = {{0, 2, 5, 10, 20, 40, 70, 120}, {120, 70, 40, 20, 10, 5, 2, 0}};
static const int PASSED_PAWN_BONUS_EG[2][8] = {{0, 5, 10, 20, 40, 90, 150, 200}, {200, 150, 90, 40, 20, 10, 5, 0}};
static const int DOUBLED_ISOLATED_PAWN_MG = -10;
static const int DOUBLED_ISOLATED_PAWN_EG = -20;

static const int BACKWARD_PAWN_PENALTY_MG[8] = {-20, -25, -30, -35, -35, -30, -25, -20};
static const int BACKWARD_PAWN_PENALTY_EG[8] = {-25, -30, -35, -40, -40, -35, -30, -25};

//Outposts (indexed by file)
static const int KNIGHT_OUTPOST_BONUS_MG[8] = {2, 4, 8, 16, 16, 8, 4, 2};
static const int KNIGHT_OUTPOST_BONUS_EG[8] = {3, 6, 10, 20, 20, 10, 6, 3};

static const int BISHOP_OUTPOST_BONUS_MG[8] = {2, 4, 8, 16, 16, 8, 4, 2};
static const int BISHOP_OUTPOST_BONUS_EG[8] = {3, 6, 10, 20, 20, 10, 6, 3};

static const int PAWN_CONNECTED_BONUS_MG[2][64] = {
    { 0, 0, 0, 0, 0, 0, 0, 0,
      2, 2, 2, 3, 3, 2, 2, 2,
      4, 4, 5, 6, 6, 5, 4, 4,
      7, 8,10,12,12,10, 8, 7,
     11,14,17,21,21,17,14,11,
     16,21,25,33,33,25,21,16,
     32,42,50,55,55,50,42,32,
      0, 0, 0, 0, 0, 0, 0, 0},
      
    { 0, 0, 0, 0, 0, 0, 0, 0,
     32,42,50,55,55,50,42,32,
     16,21,25,33,33,25,21,16,
     11,14,17,21,21,17,14,11,
      7, 8,10,12,12,10, 8, 7,
      4, 4, 5, 6, 6, 5, 4, 4,
      2, 2, 2, 3, 3, 2, 2, 2,
      0, 0, 0, 0, 0, 0, 0, 0}
};

static const int PAWN_CONNECTED_BONUS_EG[2][64] = {
    { 0, 0, 0, 0, 0, 0, 0, 0,
      4, 4, 5, 6, 6, 5, 4, 4,
      7, 8,10,12,12,10, 8, 7,
     11,14,17,21,21,17,14,11,
     16,21,25,33,33,25,21,16,
     26,31,35,43,43,35,31,26,
	 52,62,70,86,86,70,62,52,
      0, 0, 0, 0, 0, 0, 0, 0},
      
    { 0, 0, 0, 0, 0, 0, 0, 0,
     52,62,70,86,86,70,62,52,
	 26,31,35,43,43,35,31,26,
     16,21,25,33,33,25,21,16,
     11,14,17,21,21,17,14,11,
     7, 8,10,12,12,10, 8, 7,
     4, 4, 5, 6, 6, 5, 4, 4,
     0, 0, 0, 0, 0, 0, 0, 0}
};

int Evaluation::MIRROR64[64];

void Evaluation::initAll(){
	for (int i = 0; i < 64; i++){
		int r = i/8;
		int c = i % 8;
		int sqBlack = (7 - r)* 8 + c;
		MIRROR64[i] = sqBlack;
	}
}	

void Evaluation::materialBalance(const Board& board, int& mg, int& eg){
	/*
	for (int i = 0; i < 64; i++){
		mg+= PIECE_VALUES[board.board[i]];
		eg+= PIECE_VALUES[board.board[i]];
	}
	*/
	mg+= board.material[0] - board.material[1];
	eg+= board.material[0] - board.material[1];
}

void Evaluation::pieceSquaresBalance(const Board& board, int& mg, int& eg){
	//white
	U64 pieces = board.bitboards[Board::WHITE];
	while(pieces){
		int sq = numberOfTrailingZeros(pieces);
		mg+= PIECE_SQUARES_MG[board.board[sq]][sq];
		eg+= PIECE_SQUARES_END[board.board[sq]][sq];
		pieces&= pieces - 1;
	}
	
	//black
	pieces = board.bitboards[Board::BLACK];
	while(pieces){
		int sq = numberOfTrailingZeros(pieces);
		mg-= PIECE_SQUARES_MG[board.board[sq]][MIRROR64[sq]];
		eg-= PIECE_SQUARES_END[board.board[sq]][MIRROR64[sq]];
		pieces&= pieces - 1;
	}
}

// backward pawns that can be pushed will not be considered
// example, fen k7/5p2/6p1/p1p3P1/P1P5/7P/1P6/K7 w - -
void backward_pawns_chained(const Board& board, int& mg, int& eg){

	int dirs[2][2] = {{7, 64 - 9}, {9, 64 - 7}};
	int diffs[2][2] = {{7, -9}, {9, -7}};
	U64 pAttacks[2] = {0, 0};

	for (int side = 0; side < 2; side++){
		U64 pawnBB = board.bitboards[Board::PAWN | side];
		for (int i = 0; i < 2; i++){
			int *dir = dirs[i];
			int *diff = diffs[i];
			U64 wFile = BitBoardGen::WRAP_FILES[i];
			U64 attacks = BitBoardGen::circular_lsh(pawnBB, dir[side]) & ~wFile;
			pAttacks[side]|= attacks;
		}
	}

	//white
	U64 wp = board.bitboards[Board::WHITE_PAWN];	
	U64 stops = wp << 8;
	U64 spans = 0;

	while(wp){
		int sq = numberOfTrailingZeros(wp);
		spans|= BitBoardGen::FRONT_ATTACK_SPAN[Board::WHITE][sq];	
		wp&= wp - 1;
	}

	U64 wBack = (stops & pAttacks[Board::BLACK] & ~spans) >> 8;

	//Black
	U64 bp = board.bitboards[Board::BLACK_PAWN];	
	stops = bp >> 8;
	spans = 0;

	while(bp){
		int sq = numberOfTrailingZeros(bp);
		spans|= BitBoardGen::FRONT_ATTACK_SPAN[Board::BLACK][sq];
		bp&= bp - 1;
	}
	U64 bBack = (stops & pAttacks[Board::WHITE] & ~spans) << 8;
	
	// Score
	while(wBack){
		int sq = numberOfTrailingZeros(wBack);
		int file = sq & 7;
		mg+= BACKWARD_PAWN_PENALTY_MG[file];
		eg+= BACKWARD_PAWN_PENALTY_EG[file];
		wBack&= wBack - 1;
	}

	while(bBack){
		int sq = numberOfTrailingZeros(bBack);
		int file = sq & 7;
		mg+= -1 * BACKWARD_PAWN_PENALTY_MG[file];
		eg+= -1 * BACKWARD_PAWN_PENALTY_EG[file];
		bBack&= bBack - 1;
	}
}

void Evaluation::evalPawns(const Board& board, int& mg, int& eg, AttackCache *attCache){
	
	const int up_ahead[2] = {8, -8};	
	int s = 1;
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];

	//update attCache pawn attacks
	attCache->pawns[0] = ((board.bitboards[Board::WHITE_PAWN] << 9) & ~BitBoardGen::BITBOARD_FILES[0]) | ((board.bitboards[Board::WHITE_PAWN] << 7) & ~BitBoardGen::BITBOARD_FILES[7]);
	attCache->pawns[1] = ((board.bitboards[Board::BLACK_PAWN] >> 9) & ~BitBoardGen::BITBOARD_FILES[7]) | ((board.bitboards[Board::BLACK_PAWN] >> 7) & ~BitBoardGen::BITBOARD_FILES[0]);

	//update attCache all attacks
	attCache->all[0] = attCache->rooks[0] | attCache->knights[0] | attCache->bishops[0] | attCache->queens[0] | attCache->pawns[0];
	attCache->all[1] = attCache->rooks[1] | attCache->knights[1] | attCache->bishops[1] | attCache->queens[1] | attCache->pawns[1];

	//update attCache all2 attacks
	attCache->all2[0] = attCache->rooks[0] & attCache->knights[0] & attCache->bishops[0] & attCache->queens[0] & attCache->pawns[0];
	attCache->all2[1] = attCache->rooks[1] & attCache->knights[1] & attCache->bishops[1] & attCache->queens[1] & attCache->pawns[1];

	for (int side = 0; side < 2; side++){
		
		int opp = side^1;		
		U64 pawnBB = board.bitboards[Board::PAWN | side];
		U64 pawns = pawnBB;
		U64 oppPawns = board.bitboards[Board::PAWN | opp];

		while (pawns){
			int sq = numberOfTrailingZeros(pawns);
			int file = sq & 7;

			bool isolated = false;			
			
			//isolated
			if ((BitBoardGen::ADJACENT_FILES[file] & pawnBB) == 0){
				mg+= s * ISOLATED_PAWN_PENALTY_MG[file];
				eg+= s * ISOLATED_PAWN_PENALTY_EG[file];
				isolated = true;
			}
			
			//passed
			U64 frontSpan = BitBoardGen::FRONT_SPAN[side][sq];
			bool passed = false;
			
			if ((frontSpan & oppPawns) == 0){
				int r = sq >> 3;
				mg+= s * PASSED_PAWN_BONUS_MG[side][r];
				eg+= s * PASSED_PAWN_BONUS_EG[side][r];
				passed = true;
			}
			
			bool connected = false;

			//connected
			if (BitBoardGen::PAWN_CONNECTED[side][sq] & pawnBB){
				mg+= s * PAWN_CONNECTED_BONUS_MG[side][sq];
				eg+= s * PAWN_CONNECTED_BONUS_EG[side][sq];
				connected = true;
			}
			pawns&= pawns - 1;

			//test for isolated and doubled
			if (isolated && (BitBoardGen::BITBOARD_FILES[file] & pawns)){
				mg+= s * DOUBLED_ISOLATED_PAWN_MG;
				eg+= s * DOUBLED_ISOLATED_PAWN_EG;
			}

			//eval passed stockfish 57.45% +/- 1.75% in 2100 games
			if (passed){
				int r = (side == Board::WHITE) ? sq >> 3 : (7 - (sq >> 3));

				//rank 3
				if (r > 2){
					int w = (r - 2) * (r - 2) + 2;
					int blockSq = sq + up_ahead[side];
					int dopp = std::min(BitBoardGen::DISTANCE_SQS[blockSq][board.kingSQ[opp]], 5);
					int dus = std::min(BitBoardGen::DISTANCE_SQS[blockSq][board.kingSQ[side]], 5);		
					eg+= s * (5 * dopp - 2 * dus) * w;

					if (r != 6)
						eg+= -s * std::min(BitBoardGen::DISTANCE_SQS[blockSq + up_ahead[side]][board.kingSQ[side]], 5) * w;

					if (!board.board[blockSq]){
						U64 defendedSquares = BitBoardGen::SQUARES_AHEAD[side][sq];
						U64 unsafeSquares = BitBoardGen::SQUARES_AHEAD[side][sq];
						U64 squaresToQueen = BitBoardGen::SQUARES_AHEAD[side][sq];

						U64 rooks_queens = board.bitboards[Board::WHITE_ROOK] | board.bitboards[Board::BLACK_ROOK] |
										   board.bitboards[Board::WHITE_QUEEN] | board.bitboards[Board::BLACK_QUEEN];

						U64 bb = BitBoardGen::SQUARES_AHEAD[opp][sq] & rooks_queens & Magic::rookAttacksFrom(occup, sq);

						//should include pawns?
						if (!(board.bitboards[side] & bb))
							defendedSquares&= attCache->allAttacks(side);

						if (!(board.bitboards[opp] & bb))
							unsafeSquares&= attCache->allAttacks(opp) | board.bitboards[opp];

						int k = !unsafeSquares ? 20 : !(unsafeSquares & BitBoardGen::SQUARES[blockSq]) ? 9 : 0;

						if (defendedSquares == squaresToQueen)
							k+= 6;
						else if (defendedSquares & BitBoardGen::SQUARES[blockSq])
							k+= 4;

						mg+= s * k * w;
						eg+= s * k * w;

					}
				} //rank 3
			}
			//eval passed stockish


			//eval passed old score 52.22% +/- 1.19% in 4910 games
			/*
			if (passed){
				//King distance diff from passed pawn square (max val = 12)				
				int kingsDelta = BitBoardGen::DISTANCE_SQS[sq][board.kingSQ[opp]] - BitBoardGen::DISTANCE_SQS[sq][board.kingSQ[side]];
				mg+= s * 2 * kingsDelta;
				eg+= s * 10 * kingsDelta;			

				//is path to promotion clear?
				if (!(BitBoardGen::SQUARES_AHEAD[side][sq] & board.bitboards[opp])) {
					
					// more connected bonus
					if (connected){
						mg+= s * PAWN_CONNECTED_BONUS_MG[side][sq];
						eg+= s * PAWN_CONNECTED_BONUS_EG[side][sq];
					}

					//count number of enemy piece attacks (not pawns or king) at square ahead of pawn					
					U64 sqAhead = BitBoardGen::SQUARES[sq + up_ahead[side]];
					int attacks = attCache->countAttacksAt(sqAhead, opp);

					//check if square ahead has no piece enemy or king attacks (pawn attacks are not possible because our pawn is passed)
					if (!attacks && BitBoardGen::DISTANCE_SQS[sq + up_ahead[side]][board.kingSQ[opp]] > 1){
						mg+= s * 20;
						eg+= s * 40;						
					}

					//more bonus if the enemy has no attack at the promotion square	(needs: color white = 0, color black = 1)					
					U64 promoSQ = BitBoardGen::SQUARES[opp * 56 + file];					
					attacks = attCache->countAttacksAt(promoSQ, opp);

					if (!attacks && BitBoardGen::DISTANCE_SQS[opp * 56 + file][board.kingSQ[opp]] > 1){
						mg+= s * 20;
						eg+= s * 40;						
					}
				}

			} //if passed old
			*/
		}
		s = -1;
	}

	//backward pawns
	//backward_pawns_chained(board, mg, eg);
}

bool Evaluation::materialDraw(const Board& board){

	if (board.bitboards[Board::WHITE_PAWN] || board.bitboards[Board::BLACK_PAWN])
		return false;

	bool hasWQ = board.bitboards[Board::WHITE_QUEEN];
	bool hasBR = board.bitboards[Board::BLACK_ROOK];
	bool hasBQ = board.bitboards[Board::BLACK_QUEEN];
	bool hasWR = board.bitboards[Board::WHITE_ROOK];

	//no queen or rooks
	if (!hasWQ && !hasBQ && !hasWR && !hasBR){

		bool hasWB = board.bitboards[Board::WHITE_BISHOP];
		bool hasWN = board.bitboards[Board::WHITE_KNIGHT];
		bool hasBB = board.bitboards[Board::BLACK_BISHOP];
		bool hasBN = board.bitboards[Board::BLACK_KNIGHT];
		int wn = BitBoardGen::popCount(board.bitboards[Board::WHITE_KNIGHT]);
		int bn = BitBoardGen::popCount(board.bitboards[Board::BLACK_KNIGHT]);
		int wb = BitBoardGen::popCount(board.bitboards[Board::WHITE_BISHOP]);
		int bb = BitBoardGen::popCount(board.bitboards[Board::BLACK_BISHOP]);

		//no bishops
		if(!hasWB && !hasBB){
			// 2 or less knights
			if (wn < 3 && bn < 3)
				return true;
		}
		//has bishops but no knights
		else if (!hasWN && !hasBN){
			if (abs(wb - bb) < 2)
				return true;
		}//has bishops and knights
		else if((wn < 3 && !hasWB) || (wb == 1 && !hasWN)){
			if ((bn < 3 && !hasBB) || (bb == 1 && !hasBN)) {
				return true;
			}
		}
		//has rooks, no queen
	} else if (!hasWQ && !hasBQ) {
		int wr = BitBoardGen::popCount(board.bitboards[Board::WHITE_ROOK]);
		int br = BitBoardGen::popCount(board.bitboards[Board::BLACK_ROOK]);
		int wn = BitBoardGen::popCount(board.bitboards[Board::WHITE_KNIGHT]);
		int bn = BitBoardGen::popCount(board.bitboards[Board::BLACK_KNIGHT]);
		int wb = BitBoardGen::popCount(board.bitboards[Board::WHITE_BISHOP]);
		int bb = BitBoardGen::popCount(board.bitboards[Board::BLACK_BISHOP]);

        if (wr == 1 && br == 1) {
            if ((wn + wb) < 2 && (bn + bb) < 2)	{ 
            	return true; 
            }
        } else if (wr == 1 && !hasBR) {
            if ((wn + wb == 0) && (((bn + bb) == 1) || ((bn + bb) == 2))) { 
            	return true; 
            }
        } else if (br == 1 && !hasWR) {
            if ((bn + bb == 0) && (((wn + wb) == 1) || ((wn + wb) == 2))) {
            	return true; 
         	}
        }
    }
    return false;
}
									
void Evaluation::pieceOpenFile(const Board& board, int& mg, int& eg){
	
	const int pieceTypes[2] = {Board::ROOK, Board::QUEEN};
	U64 pawnBoth = board.bitboards[Board::WHITE_PAWN] | board.bitboards[Board::BLACK_PAWN];
	int s = 1;

	for (int side = 0; side < 2; side++){
		for (int p = 0; p < 2; p++){
			
			int opp = side^1;
			U64 pieces = board.bitboards[pieceTypes[p] | side];
			U64 pawnSide = board.bitboards[Board::PAWN | side];
			U64 pawnOpp = board.bitboards[Board::PAWN | opp];
			
			while (pieces){
				int sq = numberOfTrailingZeros(pieces);
				int file = sq & 7;

				if ((pawnBoth & BitBoardGen::BITBOARD_FILES[file]) == 0){
					mg+= s * OPENFILES_BONUS_MG[p][0];
					eg+= s * OPENFILES_BONUS_EG[p][0];
				}
				else if ((pawnSide & BitBoardGen::BITBOARD_FILES[file]) == 0){
					mg+= s * OPENFILES_BONUS_MG[p][1];
					eg+= s * OPENFILES_BONUS_EG[p][1];
				}
				pieces&= pieces - 1;
			}
		}
		s = -1;
	}
}

void Evaluation::kingAttack(const Board& board, int& mg, AttackCache *attCache){
	mg+= kingAttackedSide(board, Board::BLACK, attCache) - kingAttackedSide(board, Board::WHITE, attCache);
}

// Return opp attack eval on king from side
int Evaluation::kingAttackedSide(const Board& board, int side, AttackCache *attCache){
	int opp = side^1;
	U64 king = board.bitboards[Board::KING | side];
	int kingSq = numberOfTrailingZeros(king);
	U64 region = BitBoardGen::BITBOARD_KING_REGION[side][kingSq];
	U64 attackerPawns = board.bitboards[Board::PAWN | opp];
	//U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 occup = board.bitboards[side] | attackerPawns;
	
	int numAttackers = 0;
	int attackVal = 0;

	//Attack weights
	const int ROOK_AW = 137;
	const int QUEEN_AW = 115;
	const int BISHOP_AW = 11;
	const int KNIGHT_AW = 95;

	//Weight of attack by numAttackers, 0 or 1 attacker => 0 weight
	const int ATTACK_W[] = {0, 0, 30, 75, 88, 94, 97, 99};

	//rooks
	U64 rooks = board.bitboards[Board::ROOK | opp];
	int nrooks = 0;
	
	while (rooks){	
		int from = numberOfTrailingZeros(rooks);
		U64 tmpTarg = Magic::rookAttacksFrom(occup, from);

		//attack cache
		attCache->rooks[opp]|= tmpTarg;

		if (tmpTarg & region)
			nrooks++;
		rooks&= rooks - 1;
	}
	numAttackers+= nrooks;
	attackVal+= nrooks * ROOK_AW;
	
	//queens
	U64 queens = board.bitboards[Board::QUEEN | opp];
	int nqueens = 0;
	
	while (queens){	
		int from = numberOfTrailingZeros(queens);
		U64 tmpTarg = Magic::rookAttacksFrom(occup, from) | Magic::bishopAttacksFrom(occup, from);

		//attack cache
		attCache->queens[opp]|= tmpTarg;

		if (tmpTarg & region)	
			nqueens++;
		queens&= queens - 1;
	}
	
	numAttackers+= nqueens;
	attackVal+= nqueens * QUEEN_AW;
	
	//bishops
	U64 bishops = board.bitboards[Board::BISHOP | opp];
	int nbishops = 0;
	
	while (bishops){
		int from = numberOfTrailingZeros(bishops);
		U64 tmpTarg = Magic::bishopAttacksFrom(occup, from);

		//attack cache
		attCache->bishops[opp]|=  tmpTarg;

		if (tmpTarg & region)
			nbishops++;
		bishops&= bishops - 1;
	}
	
	numAttackers+= nbishops;
	attackVal+= nbishops * BISHOP_AW;
	
	//knights
	U64 knights = board.bitboards[Board::KNIGHT | opp];
	int nknights = 0;
	
	while (knights){
		int from = numberOfTrailingZeros(knights);
		U64 tmpTarg = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from];

		//attack cache
		attCache->knights[opp]|=  tmpTarg;

		if (tmpTarg & region)
			nknights++;
		knights&= knights - 1;
	}
	
	numAttackers+= nknights;
	attackVal+= nknights * KNIGHT_AW;

	//Pawn storm
	U64 kingHalf = (kingSq % 8 < 4) ? BitBoardGen::QUEENSIDE_MASK : BitBoardGen::KINGSIDE_MASK;
	
	attackerPawns&= kingHalf;
	int stormBonus = 0;
	int nStorm = BitBoardGen::popCount(attackerPawns);

	if (nStorm > 1){
		while(attackerPawns){
			int from = numberOfTrailingZeros(attackerPawns);
			stormBonus+= (opp == Board::WHITE) ? STORM_MAP[from] : STORM_MAP[MIRROR64[from]];
			attackerPawns&= attackerPawns - 1;
		}
	}

	// two or more pawns
	attackVal+= stormBonus;
	numAttackers = numAttackers > 7 ? 7 : numAttackers;
	attackVal*= ATTACK_W[numAttackers];
	
	return attackVal/150;
}													

static const int king_shelter_bonus1 = 8;
static const int king_shelter_bonus2 = 4;
//one sq ahead region
static const int king_shelter_attacked_penalty1[] = {0, 0, -16, -48};
//two squares ahead region
static const int king_shelter_attacked_penalty2[] = {0, 0, -8, -24};
//opp connected pawns attacking shelter
static const int king_connected_attack_penalty = -5;

// if has shelter, evaluate?
void Evaluation::kingShelter(const Board& board, int& mg){
	
	int s = 1;
	for (int side = 0; side < 2; side++){
		
		int opp = side^1;
		int ks = board.kingSQ[side];
		U64 front1 = BitBoardGen::BITBOARD_KING_AHEAD[side][ks][0];
		U64 front2 = BitBoardGen::BITBOARD_KING_AHEAD[side][ks][1];
		U64 myPawns = board.bitboards[Board::PAWN | side];
		U64 oppPawns = board.bitboards[Board::PAWN | opp];
		
		//defense pawns
		U64 shelter = front1 & myPawns;
		mg+= s * BitBoardGen::popCount(shelter) * king_shelter_bonus1;
		
		shelter = front2 & myPawns;
		mg+= s * BitBoardGen::popCount(shelter) * king_shelter_bonus2;
		
		//enemy pawns
		U64 oppPawnsAttacking = 0;
		shelter = front1 & oppPawns;
		oppPawnsAttacking|= shelter;
		mg+= s * king_shelter_attacked_penalty1[BitBoardGen::popCount(shelter)];
		
		shelter = front2 & oppPawns;
		oppPawnsAttacking|= shelter;
		mg+= s * king_shelter_attacked_penalty2[BitBoardGen::popCount(shelter)];

		while(oppPawnsAttacking){
			int sq = numberOfTrailingZeros(oppPawnsAttacking);
			if (BitBoardGen::PAWN_CONNECTED[opp][sq] & oppPawns){
				mg+= s * king_connected_attack_penalty;
			}
			oppPawnsAttacking&= oppPawnsAttacking - 1;
		}
		
		s = -1;
	}
}

static const int tropism_enemy[2][4] = {
	{Board::BLACK_QUEEN, Board::BLACK_ROOK, Board::BLACK_KNIGHT, Board::BLACK_BISHOP},
	{Board::WHITE_QUEEN, Board::WHITE_ROOK, Board::WHITE_KNIGHT, Board::WHITE_BISHOP}
};

void Evaluation::kingTropism(const Board& board, int& mg){
	int s = 1;
	for (int side = 0; side < 2; side++){
		U64 enemy = board.bitboards[side^1];
		int kingSQ = board.kingSQ[side];

		while(enemy){
			int sq = numberOfTrailingZeros(enemy);
			mg-= s * 4 * (14 - BitBoardGen::DISTANCE_MAN[sq][kingSQ]) * (board.board[sq] == tropism_enemy[side][0]);
        	mg-= s * 3 * (14 - BitBoardGen::DISTANCE_MAN[sq][kingSQ]) * (board.board[sq] == tropism_enemy[side][1]);
        	mg-= s * 2 * (14 - BitBoardGen::DISTANCE_MAN[sq][kingSQ]) * (board.board[sq] == tropism_enemy[side][2]);
        	mg-= s * 2 * (14 - BitBoardGen::DISTANCE_MAN[sq][kingSQ]) * (board.board[sq] == tropism_enemy[side][3]);
			enemy&= enemy - 1;
		}
		s = -1;
	}
}

Board Evaluation::mirrorBoard(Board& board){
	Board mBoard;
	int tmpCastle = 0;
	int tmpEP = 0;

	//Castle
	if (board.state.white_can_castle_ks()) tmpCastle |= BoardState::BK_CASTLE;
	if (board.state.white_can_castle_qs()) tmpCastle |= BoardState::BQ_CASTLE;
	if (board.state.black_can_castle_ks()) tmpCastle |= BoardState::WK_CASTLE;
	if (board.state.black_can_castle_qs()) tmpCastle |= BoardState::WQ_CASTLE;

	//Ep square
	if (board.state.epSquare != 0)
		tmpEP = Evaluation::MIRROR64[board.state.epSquare];

	int mPieces[] = {0, 0, Board::BLACK_PAWN, Board::WHITE_PAWN, Board::BLACK_KNIGHT, Board::WHITE_KNIGHT, 
		Board::BLACK_BISHOP, Board::WHITE_BISHOP, Board::BLACK_ROOK, Board::WHITE_ROOK, 
		Board::BLACK_QUEEN, Board::WHITE_QUEEN, Board::BLACK_KING, Board::WHITE_KING};
	
	//mirror pieces
	for (int i = 0; i < 64; i++)
		mBoard.board[i] = mPieces[board.board[Evaluation::MIRROR64[i]]];

	//setup bitboards
	for (int s = 0; s < 64; s++){
		if (mBoard.board[s] == Board::EMPTY)
			continue;
		mBoard.bitboards[mBoard.board[s]] |= BitBoardGen::ONE << s;
		mBoard.bitboards[mBoard.board[s] & 1] |= BitBoardGen::ONE << s;
	}

	//material
	mBoard.material[0] = board.material[1];
	mBoard.material[1] = board.material[0];

	//king square
	mBoard.kingSQ[0] = numberOfTrailingZeros(mBoard.bitboards[Board::WHITE_KING]);
	mBoard.kingSQ[1] = numberOfTrailingZeros(mBoard.bitboards[Board::BLACK_KING]);

	int side = board.state.currentPlayer;
	mBoard.histPly = board.histPly;	
	mBoard.state = BoardState(tmpEP, board.state.halfMoves, side^1, tmpCastle, Zobrist::getKey(mBoard));
	mBoard.ply = 0;
	mBoard.hashTable = NULL;

	return mBoard;
}

int Evaluation::countMaterial(const Board& board, int piece){
	return BitBoardGen::popCount(board.bitboards[piece]);
}

int Evaluation::materialValueSide(const Board& board, int side){
	int mat = 0;

	for (int i = 0; i < 64; i++){
		if (! board.board[i])
			continue;
		if ((board.board[i] & 1) == side)
			mat+= PIECE_VALUES[board.board[i]];
	}
	return abs(mat) - KING_VAL;
}

//Phase
static const int pawn_phase = 0;
static const int knight_phase = 1;
static const int bishop_phase = 1;
static const int rook_phase = 2;
static const int queen_phase = 4;
static const int total_phase = 24;

int Evaluation::get_phase(const Board& board){
	int phase = total_phase;
	
	for (int side = 0; side < 2; side++){	
		phase-= countMaterial(board, Board::PAWN | side) * pawn_phase;
		phase-= countMaterial(board, Board::KNIGHT | side) * knight_phase;
		phase-= countMaterial(board, Board::BISHOP | side) * bishop_phase;
		phase-= countMaterial(board, Board::ROOK | side) * rook_phase;
		phase-= countMaterial(board, Board::QUEEN | side) * queen_phase;
	}
	return (phase * 256 + (total_phase / 2)) / total_phase;
}

void Evaluation::testEval(std::string test_file){
	std::ifstream file(test_file);
    std::string line; 
	int n = 0;

	printf("Running eval test\n");

    while (std::getline(file, line)){
    	Board board = FenParser::parseFEN(line);    	    	
    	Board mBoard = mirrorBoard(board);
    	int ev1 = evaluate(board, board.state.currentPlayer);
    	int ev2 = evaluate(mBoard, mBoard.state.currentPlayer);

    	//printf("Eval[%d] = %d\n", n, ev1);

    	if (ev1 != ev2){
    		std::cout << "Eval test fail at FEN: " << line << std::endl;
    		return;
    	}
    	n++;
    }
    printf("Evaluation ok: tested %d positions.\n", n);
}

void Evaluation::evalBishops(const Board& board, int& mg, int& eg){

	int wb_cnt = BitBoardGen::popCount(board.bitboards[Board::WHITE_BISHOP]);
	int bb_cnt = BitBoardGen::popCount(board.bitboards[Board::BLACK_BISHOP]);

	//Bishop pair bonus
	if (wb_cnt > 1){
		mg+= BISHOP_PAIR_MG;
		eg+= BISHOP_PAIR_EG;
	}
	if (bb_cnt > 1){
		mg-= BISHOP_PAIR_MG;
		eg-= BISHOP_PAIR_EG;
	}

	/*
	//Pawns of same color penalty
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	
	int s = 1;
	for (int side = 0; side < 2; side++){
		U64 bsh = board.bitboards[Board::BISHOP | side];
		U64 blockedPawns;
		U64 pawnsSide = board.bitboards[Board::PAWN | side];

		if (side == Board::WHITE)
			blockedPawns = board.bitboards[Board::WHITE_PAWN] & (occup >> 8);
		else
			blockedPawns = board.bitboards[Board::BLACK_PAWN] & (occup << 8);
	
		while(bsh){
			int color =  BitBoardGen::COLOR_OF_SQ[numberOfTrailingZeros(bsh)];
			U64 pawnsSameColor = pawnsSide & BitBoardGen::LIGHT_DARK_SQS[color] & blockedPawns;
			int cntPenalty = BitBoardGen::popCount(pawnsSameColor);// * (1 + BitBoardGen::popCount(blockedPawns & BitBoardGen::CENTER_FILES));

			mg+= s * BISHOP_PAWN_PENALTY_MG * cntPenalty;
			eg+= s * BISHOP_PAWN_PENALTY_EG * cntPenalty;
			bsh&= bsh - 1;
		}
		s = -1;
	}
	*/
}

//Double rooks on 7th bonus (extra bonus if side has queen)
void Evaluation::evalRooks(const Board& board, int& mg, int& eg){
	int s = 1;
	const U64 seventh_rank[2] = {BitBoardGen::BITBOARD_RANKS[6], BitBoardGen::BITBOARD_RANKS[1]};
	
	//double rooks on 7th
	for (int side = 0; side < 2; side++){
		U64 rooks = board.bitboards[Board::ROOK | side];
		
		if (BitBoardGen::popCount(rooks & seventh_rank[side]) > 1){
			mg+= s * D_ROOK_7_MG;
			eg+= s * D_ROOK_7_EG;
			
			//More bonus if side has queen
			mg+= board.bitboards[Board::QUEEN | side] ? s * D_ROOK_7_Q : 0;
			eg+= board.bitboards[Board::QUEEN | side] ? s * D_ROOK_7_Q : 0;
		}
		s = -1;
	}
}

//Mobility
static const int MOB_N[2][9] = {{-29, -9, -3, -1, 3, 6, 8, 12, 14}, {-30, -6, -2, 2, 5, 8, 11, 14, 15}};
static const int MOB_B[2][14] = {{-21, -6, 6, 12, 15, 21, 23, 26, 29, 32, 32, 33, 33, 33}, {-32, -7, 3, 7, 11, 16, 19, 23, 30, 33, 35, 37, 40, 42}};
static const int MOB_R[2][15] = {{-23, -7, -5, -4, -3, -1, 4, 8, 12, 12, 14, 15, 17, 20, 23}, {-36, -8, 8, 19, 25, 36, 42, 47, 53, 57, 63, 65, 67, 68, 68}};
static const int MOB_Q[2][28] = {{-25, -8, 0, 1, 5, 8, 12, 15, 16, 19, 21, 24, 25, 27, 27, 28, 29, 30, 32, 34, 35, 39, 40, 40, 41, 43, 44, 45}, {-40, -2, 6, 9, 18, 23, 28, 32, 34, 38, 42, 45, 46, 46, 49, 51, 52, 54, 56, 58, 62, 66, 70, 75, 79, 81, 83, 84}};		

/*
static const int MOB_N[2][9] = {{-75, -56, -9, -2, 6, 15, 22, 30, 36}, {-76, -54, -26, -10, 5, 11, 26, 28, 29}};
static const int MOB_B[2][14] = {{-48, -21 ,16, 26, 37, 51, 54, 63, 65, 71, 79, 81, 92, 97}, {-58, -19, -2, 12, 22, 42, 54, 58, 63, 70, 74, 86, 90, 94}};
static const int MOB_R[2][15] = {{-56, -25, -11, -5, -4, -1, 8, 14, 21, 23, 31, 32, 43, 49, 59}, {-78, -18, 26, 55, 70, 81, 109, 120, 128, 143, 154, 160, 165, 168, 169}};
static const int MOB_Q[2][28] = {{-40, -25, 2, 4, 14, 24, 25, 40, 43, 47, 54, 56, 60, 70, 72, 73, 75, 
					77, 85, 94, 99, 108, 112, 113, 118, 119, 123, 128}, 
					{-35, -12, 7, 19, 37, 55, 62, 76, 79, 87, 94, 102, 111, 116, 118, 122,
					128, 130, 133, 136, 140, 157, 158, 161, 174, 177, 191, 199}};
*/


static const int dirs[2][2] = {{7, 64 - 9}, {9, 64 - 7}};
static const int diffs[2][2] = {{7, -9}, {9, -7}};
static const U64 lowRanks[2] = {BitBoardGen::BITBOARD_RANKS[1] | BitBoardGen::BITBOARD_RANKS[2], BitBoardGen::BITBOARD_RANKS[6] | BitBoardGen::BITBOARD_RANKS[5]};

void Evaluation::mobility(const Board& board, int& mg, int& eg, AttackCache *attCache){
	
	U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
	int s = 1;
	const int scale = 1;
	const int scale_q = 2;
	int n = 0;

	for (int side = 0; side < 2; side++){
		
		int opp = side^1;
		U64 oppPawnBB = board.bitboards[Board::PAWN | opp];
		U64 oppPawnAttacks = 0;
		U64 blockedLowPawns = 0;

		if (side == Board::WHITE){
			oppPawnAttacks = ((oppPawnBB >> 9) & ~BitBoardGen::BITBOARD_FILES[7]) | ((oppPawnBB >> 7) & ~BitBoardGen::BITBOARD_FILES[0]);
			blockedLowPawns = board.bitboards[Board::WHITE_PAWN] & ((occup >> 8) | lowRanks[side]);			
		} else{			
			oppPawnAttacks = ((oppPawnBB << 9) & ~BitBoardGen::BITBOARD_FILES[0]) | ((oppPawnBB << 7) & ~BitBoardGen::BITBOARD_FILES[7]);
			blockedLowPawns = board.bitboards[Board::BLACK_PAWN] & ((occup << 8) | lowRanks[side]);								
		}
		
		U64 mobilityArea = ~(blockedLowPawns | board.bitboards[Board::KING | side] | board.bitboards[Board::QUEEN | side] | oppPawnAttacks); 

		//knights
		if (board.bitboards[Board::KNIGHT | side]){
			n = BitBoardGen::popCount(attCache->knights[side] & mobilityArea);
			mg+= s * MOB_N[0][n]/scale;
			eg+= s * MOB_N[1][n]/scale;
		}
		
		//bishops		
		if (board.bitboards[Board::BISHOP | side]){
			n = BitBoardGen::popCount(attCache->bishops[side] & mobilityArea);
			mg+= s * MOB_B[0][n]/scale;
			eg+= s * MOB_B[1][n]/scale;
		}
		
		//rooks
		if (board.bitboards[Board::ROOK | side]){
			n = BitBoardGen::popCount(attCache->rooks[side] & mobilityArea);
			mg+= s * MOB_R[0][n]/scale;
			eg+= s * MOB_R[1][n]/scale;
		}
		
		//queen		
		if (board.bitboards[Board::QUEEN | side]){
			n = BitBoardGen::popCount(attCache->queens[side] & mobilityArea);
			mg+= s * MOB_Q[0][n]/scale_q;
			eg+= s * MOB_Q[1][n]/scale_q;
		}
		
		s = -1;
	}
}

ending_type Evaluation::get_ending(const Board& board){
	
	//KBN vs K
	for (int side = 0; side < 2; side++){
		int opp = side^1;
		if (BitBoardGen::popCount(board.bitboards[side]) == 1){
			if (BitBoardGen::popCount(board.bitboards[opp]) == 3){
				if (board.bitboards[Board::KNIGHT | opp] && board.bitboards[Board::BISHOP | opp]){
					return KNB_K;
				}
			}
		}
	}
	return OTHER_ENDING;
}

//K vs KBN
//k7/3bn3/8/4K3/8/8/8/8 b - -
int Evaluation::evalKBN_K(const Board& board, int side){

	int mg = 0;
	int eg = 0;
	int bishop_color = -1;
	
	if (board.bitboards[Board::WHITE_BISHOP]){
		U64 bishop = board.bitboards[Board::WHITE_BISHOP];
		bishop_color = BitBoardGen::COLOR_OF_SQ[numberOfTrailingZeros(bishop)];
	} else {
		U64 bishop = board.bitboards[Board::BLACK_BISHOP];
		bishop_color = BitBoardGen::COLOR_OF_SQ[numberOfTrailingZeros(bishop)];
	}
	
	assert(bishop_color > 0);
	int w_ks = numberOfTrailingZeros(board.bitboards[Board::WHITE_KING]);
	int b_ks = numberOfTrailingZeros(board.bitboards[Board::BLACK_KING]);
	
	materialBalance(board, mg, eg);
	mg+= KNB_K_MATE[bishop_color][w_ks];
	eg+= KNB_K_MATE[bishop_color][w_ks];
	mg-= KNB_K_MATE[bishop_color][b_ks];
	eg-= KNB_K_MATE[bishop_color][b_ks];
	
	int phase = get_phase(board);
	int eval = ((mg * (256 - phase)) + (eg * phase))/256;
	
	return side == Board::WHITE ? eval : -eval;
}

void Evaluation::outposts(const Board& board, int&mg, int&eg){
	int s = 1;
	const int min_rank[2] = {3, 1};
	const int max_rank[2] = {6, 4};

	for (int side = 0; side < 2; side++){

		int opp = side^1;

		// Knights
		U64 kn = board.bitboards[Board::KNIGHT | side];
		U64 myPawns = board.bitboards[Board::PAWN | side];
		//test remove blocked pawns from set
		U64 enemyPawns = board.bitboards[Board::PAWN | opp];

		while (kn){
			int sq = numberOfTrailingZeros(kn);
			int file = sq & 7;
			int rank = sq >> 3;

			if (rank >= min_rank[side] && rank <= max_rank[side]){
				if ((BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][sq] & myPawns) && !(enemyPawns & BitBoardGen::FRONT_ATTACK_SPAN[side][sq])){
					mg+= s * KNIGHT_OUTPOST_BONUS_MG[file];
					eg+= s * KNIGHT_OUTPOST_BONUS_EG[file];
				}
			}			
			kn&= kn - 1;
		}

		// Bishops
		U64 bishops = board.bitboards[Board::BISHOP | side];

		while (bishops){
			int sq = numberOfTrailingZeros(bishops);
			int file = sq & 7;
			int rank = sq >> 3;

			if (rank >= min_rank[side] && rank <= max_rank[side]){
				if ((BitBoardGen::BITBOARD_PAWN_ATTACKS[opp][sq] & myPawns) && !(enemyPawns & BitBoardGen::FRONT_ATTACK_SPAN[side][sq])){
					mg+= s * BISHOP_OUTPOST_BONUS_MG[file];
					eg+= s * BISHOP_OUTPOST_BONUS_EG[file];
				}
			}
			
			bishops&= bishops - 1;
		}
		s = -1;
	}
}

//From Ethereal engine
void Evaluation::threats(const Board& board, int& mg, int& eg, AttackCache *attCache){
	int s = 1;
	int count;
	const U64 rank3[2] = {BitBoardGen::BITBOARD_RANKS[2], BitBoardGen::BITBOARD_RANKS[5]};
	const U64 pawnAttacks[2] = {
		//((board.bitboards[Board::WHITE_PAWN] << 9) & ~BitBoardGen::BITBOARD_FILES[0]) | ((board.bitboards[Board::WHITE_PAWN] << 7) & ~BitBoardGen::BITBOARD_FILES[7]),
		//((board.bitboards[Board::BLACK_PAWN] >> 9) & ~BitBoardGen::BITBOARD_FILES[7]) | ((board.bitboards[Board::BLACK_PAWN] >> 7) & ~BitBoardGen::BITBOARD_FILES[0])
		attCache->pawns[0], attCache->pawns[1]
	};

	for (int side = 0; side < 2; side++){
		int opp = side^1;

		U64 friendly = board.bitboards[side];
		U64 enemy = board.bitboards[opp];
		U64 occup = friendly | enemy;

		U64 pawns = board.bitboards[Board::PAWN | side];
		U64 knights = board.bitboards[Board::KNIGHT | side];
		U64 bishops = board.bitboards[Board::BISHOP | side];
		U64 rooks = board.bitboards[Board::ROOK | side];
		U64 queens = board.bitboards[Board::QUEEN | side];

		U64 attackByPawns = pawnAttacks[opp];
		U64 r3 = rank3[side];

		U64 attackByMinors = attCache->knights[opp] | attCache->bishops[opp];
		U64 attackByMajors = attCache->rooks[opp] | attCache->queens[opp];

		U64 poorlyDefended = (attCache->allAttacks(opp) & ~attCache->allAttacks(side))
							 | (attCache->attacks2(opp) & ~attCache->attacks2(side) & ~pawnAttacks[side]);

		U64 overloaded = (knights | bishops | rooks | queens)
						& attCache->allAttacks(side) & ~attCache->attacks2(side)
						& attCache->allAttacks(opp) & ~attCache->attacks2(opp);
		
		//maybe add pawn attacks to attCache?
		U64 pushThreat = ~occup & (side == Board::WHITE ? (pawns << 8) : (pawns >> 8));
		pushThreat|= ~occup & (side == Board::WHITE ? ((pushThreat & ~attackByPawns & r3) << 8) : ((pushThreat & ~attackByPawns & r3) >> 8));
		pushThreat&= ~attackByPawns & (attCache->allAttacks(side) | ~attCache->allAttacks(opp));

		//attacks bu push
		if (side == Board::WHITE){
			pushThreat = ((pushThreat << 9) & ~BitBoardGen::BITBOARD_FILES[0]) | ((pushThreat << 7) & ~BitBoardGen::BITBOARD_FILES[7]);
		} else{
			pushThreat = ((pushThreat >> 9) & ~BitBoardGen::BITBOARD_FILES[7]) | ((pushThreat >> 7) & ~BitBoardGen::BITBOARD_FILES[0]);	
		}
		
		pushThreat&= (enemy & ~pawnAttacks[side]);		

		//poorly supported pawns
		// count = BitBoardGen::popCount(pawns & ~attackByPawns & poorlyDefended);
		// mg+= s * count * (-15);
		// eg+= s * count * (-30);

		//threat against our minors		 
		count = BitBoardGen::popCount((knights | bishops) & attackByPawns);
		mg+= s * count * (-28);
		eg+= s * count * (-20);

		//penalty for minor agains minor
		count = BitBoardGen::popCount((knights | bishops) & attackByMinors);
		mg+= s * count * (-30);
		eg+= s * count * (-40);

		//penalty for all major threats against poorly supported minors
		// count = BitBoardGen::popCount((knights | bishops) & poorlyDefended & attackByMajors);
		// mg+= s * count * (-15);
		// eg+= s * count * (-30);

		 //penalty for pawn and minor threats against our rooks
    	count = BitBoardGen::popCount(rooks & (attackByPawns | attackByMinors));
    	mg+= s * count * (-30);
		eg+= s * count * (-10);

		 //penalty for any threat against our queens
    	count = BitBoardGen::popCount(queens & attCache->allAttacks(opp));
    	mg+= s * count * (-35);
		eg+= s * count * (-15);

		//penalty for any overloaded minors or majors
		// count = BitBoardGen::popCount(overloaded);
		// mg+= s * count * (-8);
		// eg+= s * count * (-16);

		//bonus for giving threats by safe pawn pushes
		count = BitBoardGen::popCount(pushThreat);
		mg+= s * count * 16;
		eg+= s * count * 20;

		s = -1;
	}
}


static AttackCache attCache;
int Evaluation::evaluate(const Board& board, int side){

	/* TODO
	if (materialDraw())
		if (pawns){ return scale eval }
	*/
	if (materialDraw(board))
		return 0;
	/*else if (get_ending(board) == KNB_K)
		return evalKBN_K(board, side);
	*/
	
	int mg = 0;
	int eg = 0;

	// reset attack cache
	attCache.reset();
	
	materialBalance(board, mg, eg);
	pieceSquaresBalance(board, mg, eg);	
	pieceOpenFile(board, mg, eg);
	kingAttack(board, mg, &attCache);
	//kingTropism(board, mg);
	evalPawns(board, mg, eg, &attCache);
	kingShelter(board, mg);
	evalBishops(board, mg, eg);
	evalRooks(board, mg, eg);
	outposts(board, mg, eg);
	//mobility(board, mg, eg, &attCache);
	threats(board, mg, eg, &attCache);
	
	int phase = get_phase(board);
	int eval = ((mg * (256 - phase)) + (eg * phase))/256;
	
	return side == Board::WHITE ? eval : -eval;
}


// int main(){
// 	BitBoardGen::initAll();
// 	Magic::magicArraysInit();
// 	Zobrist::init_keys();
// 	Evaluation::initAll();
// 	Board board = FenParser::parseFEN("k7/5p2/6p1/p1p3P1/P1P4P/1P6/8/K7 w - -");
// 	board.print();
// 	int m = 0, e = 0;
// 	backward_pawns_chained(board, m, e);
// 	printf("m=%d e=%d\n", m, e);
// 	return 0;
// }
