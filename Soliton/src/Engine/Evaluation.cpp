#include "Engine/Evaluation.h"
#include "Engine/defs.h"
#include "Engine/Zobrist.h"
#include "Engine/Magic.h"
#include <fstream>
#include <iostream>

int Evaluation::PIECE_SQUARES_MG[14][64];
int Evaluation::PIECE_SQUARES_END[14][64];
int Evaluation::PHASE_INC[14];
int Evaluation::MIRROR64[64] = {
    56, 57, 58, 59, 60, 61, 62, 63,
    48, 49, 50, 51, 52, 53, 54, 55,
    40, 41, 42, 43, 44, 45, 46, 47,
    32, 33, 34, 35, 36, 37, 38, 39,
    24, 25, 26, 27, 28, 29, 30, 31,
    16, 17, 18, 19, 20, 21, 22, 23,
     8,  9, 10, 11, 12, 13, 14, 15,
     0,  1,  2,  3,  4,  5,  6,  7
};

// Used in Board
int Evaluation::PIECE_VALUES[14] = { 0, 0, PAWN_VAL, -PAWN_VAL, KNIGHT_VAL, -KNIGHT_VAL, BISHOP_VAL,
                    -BISHOP_VAL, ROOK_VAL, -ROOK_VAL, QUEEN_VAL, -QUEEN_VAL, KING_VAL, -KING_VAL };

// ============================================================================
// PIECE SQUARE TABLES
// Format: 0..63 (A1..H8).
// The First line is RANK 1. The Last line is RANK 8.
// ============================================================================

// PAWN
const int mg_pawn_table[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    5,  10,  10, -20, -20,  10,  10,   5,
    5,  -5, -10,   0,   0, -10,  -5,   5,
    0,   0,   0,  20,  20,   0,   0,   0,
    5,   5,  10,  25,  25,  10,   5,   5,
    10,  10,  20,  30,  30,  20,  10,  10,
    50,  50,  50,  50,  50,  50,  50,  50,
    0,   0,   0,   0,   0,   0,   0,   0
};

const int mg_knight_table[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50
};

const int mg_bishop_table[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

const int mg_rook_table[64] = {
    0,   0,   0,   5,   5,   0,   0,   0,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
    5,  10,  10,  10,  10,  10,  10,   5,
    0,   0,   0,   0,   0,   0,   0,   0
};

const int mg_queen_table[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -10,   5,   5,   5,   5,   5,   0, -10,
     0,   0,   5,   5,   5,   5,   0,  -5,
    -5,   0,   5,   5,   5,   5,   0,  -5,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20
};

const int mg_king_table[64] = {
    20,  30,  10,   0,   0,  10,  30,  20,
    20,  20,   0,   0,   0,   0,  20,  20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30
};

const int eg_king_table[64] = {
    -50, -30, -30, -30, -30, -30, -30, -50,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -50, -40, -30, -20, -20, -30, -40, -50
};

//Piece on open files
static const int R_OPEN_MG = 14;
static const int R_OPEN_EG = 20;
static const int R_SOPEN_MG = 7;
static const int R_SOPEN_EG = 10;
static const int Q_OPEN_MG = 6;
static const int Q_OPEN_EG = 8;
static const int Q_SOPEN_MG = 3;
static const int Q_SOPEN_EG = 4;
static const int OPENFILES_BONUS_MG[2][2] = {{R_OPEN_MG, R_SOPEN_MG},{Q_OPEN_MG, Q_SOPEN_MG}}; 
static const int OPENFILES_BONUS_EG[2][2] = {{R_OPEN_EG, R_SOPEN_EG},{Q_OPEN_EG, Q_SOPEN_EG}};

// Outposts (indexed by file)
// TODO add rank bonus?
static const int KNIGHT_OUTPOST_BONUS_MG[8] = {2, 4, 8, 16, 16, 8, 4, 2};
static const int KNIGHT_OUTPOST_BONUS_EG[8] = {3, 6, 10, 20, 20, 10, 6, 3};
static const int BISHOP_OUTPOST_BONUS_MG[8] = {2, 4, 8, 16, 16, 8, 4, 2};
static const int BISHOP_OUTPOST_BONUS_EG[8] = {3, 6, 10, 20, 20, 10, 6, 3};
static const int KNIGHT_HOLE_BONUS_MG = 20;
static const int KNIGHT_HOLE_BONUS_EG = 30;
static const int BISHOP_HOLE_BONUS_MG = 15;
static const int BISHOP_HOLE_BONUS_EG = 25;

//Pawn Structure
static const int ISOLATED_PAWN_PENALTY_MG[8] = {-5, -7, -10, -10, -10, -10, -7, -5};
static const int ISOLATED_PAWN_PENALTY_EG[8] = {-10, -14, -20, -20, -20, -20, -14, -10};
static const int PASSED_PAWN_BONUS_MG[2][8] = {{0, 2, 5, 10, 20, 40, 70, 120}, {120, 70, 40, 20, 10, 5, 2, 0}};
static const int PASSED_PAWN_BONUS_EG[2][8] = {{0, 5, 10, 20, 40, 90, 150, 200}, {200, 150, 90, 40, 20, 10, 5, 0}};
static const int DOUBLED_ISOLATED_PAWN_MG = -10;
static const int DOUBLED_ISOLATED_PAWN_EG = -20;
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

// Pawn storm
static const int STORM_MAP[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    10, 10, 20,  0,  0, 20, 10, 10,
    15, 20, 50, 15, 15, 50, 20, 15,
    30, 50, 60, 50, 50, 60, 50, 30,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0
};

void Evaluation::initAll() {
    // 1. Initialize Phase Increments
    for (int i = 0; i < 14; i++) PHASE_INC[i] = 0;
    PHASE_INC[Board::WHITE_KNIGHT] = 1; PHASE_INC[Board::BLACK_KNIGHT] = 1;
    PHASE_INC[Board::WHITE_BISHOP] = 1; PHASE_INC[Board::BLACK_BISHOP] = 1;
    PHASE_INC[Board::WHITE_ROOK] = 2; PHASE_INC[Board::BLACK_ROOK] = 2;
    PHASE_INC[Board::WHITE_QUEEN] = 4; PHASE_INC[Board::BLACK_QUEEN] = 4;

    // 2. Initialize Tables
    for (int i = 0; i < 64; i++) {
        // Defaults: use MG table for End if no specific table exists
        // Pawns
        PIECE_SQUARES_MG[Board::WHITE_PAWN][i] = mg_pawn_table[i];
        PIECE_SQUARES_MG[Board::BLACK_PAWN][i] = mg_pawn_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_PAWN][i] = mg_pawn_table[i];
        PIECE_SQUARES_END[Board::BLACK_PAWN][i] = mg_pawn_table[MIRROR64[i]];

        // Knights
        PIECE_SQUARES_MG[Board::WHITE_KNIGHT][i] = mg_knight_table[i];
        PIECE_SQUARES_MG[Board::BLACK_KNIGHT][i] = mg_knight_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_KNIGHT][i] = mg_knight_table[i];
        PIECE_SQUARES_END[Board::BLACK_KNIGHT][i] = mg_knight_table[MIRROR64[i]];

        // Bishops
        PIECE_SQUARES_MG[Board::WHITE_BISHOP][i] = mg_bishop_table[i];
        PIECE_SQUARES_MG[Board::BLACK_BISHOP][i] = mg_bishop_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_BISHOP][i] = mg_bishop_table[i];
        PIECE_SQUARES_END[Board::BLACK_BISHOP][i] = mg_bishop_table[MIRROR64[i]];

        // Rooks
        PIECE_SQUARES_MG[Board::WHITE_ROOK][i] = mg_rook_table[i];
        PIECE_SQUARES_MG[Board::BLACK_ROOK][i] = mg_rook_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_ROOK][i] = mg_rook_table[i];
        PIECE_SQUARES_END[Board::BLACK_ROOK][i] = mg_rook_table[MIRROR64[i]];

        // Queens
        PIECE_SQUARES_MG[Board::WHITE_QUEEN][i] = mg_queen_table[i];
        PIECE_SQUARES_MG[Board::BLACK_QUEEN][i] = mg_queen_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_QUEEN][i] = mg_queen_table[i];
        PIECE_SQUARES_END[Board::BLACK_QUEEN][i] = mg_queen_table[MIRROR64[i]];

        // Kings (Specific END table)
        PIECE_SQUARES_MG[Board::WHITE_KING][i] = mg_king_table[i];
        PIECE_SQUARES_MG[Board::BLACK_KING][i] = mg_king_table[MIRROR64[i]];
        PIECE_SQUARES_END[Board::WHITE_KING][i] = eg_king_table[i];
        PIECE_SQUARES_END[Board::BLACK_KING][i] = eg_king_table[MIRROR64[i]];
    }
}

/* Evaluation Features */
// material
void Evaluation::materialBalance(const Board& board, int& mg, int& eg){
	mg+= board.material[Board::WHITE] - board.material[Board::BLACK];
	eg+= board.material[Board::WHITE] - board.material[Board::BLACK];
}

void Evaluation::pieceSquares(const Board& board, int& mg, int& eg, int &gamePhase){
    U64 pieces = board.bitboards[Board::WHITE];

    while (pieces) {
        int sq = numberOfTrailingZeros(pieces);
        int piece = board.board[sq];

        mg+= PIECE_SQUARES_MG[piece][sq];
        eg+= PIECE_SQUARES_END[piece][sq];
        gamePhase += PHASE_INC[piece];
        pieces &= pieces - 1;
    }

    pieces = board.bitboards[Board::BLACK];
    while (pieces) {
        int sq = numberOfTrailingZeros(pieces);
        int piece = board.board[sq];

        // Tables for Black are already mirrored in initAll, 
        mg-= PIECE_SQUARES_MG[piece][sq];
        eg-= PIECE_SQUARES_END[piece][sq];
        gamePhase += PHASE_INC[piece];
        pieces &= pieces - 1;
    }
}

void Evaluation::computeAttacks(const Board& board, EvalInfo& ei){
    // inits occupancy BB
    U64 occup = board.bitboards[Board::WHITE] | board.bitboards[Board::BLACK];
    ei.occup = occup;
    const int dirs[2][2] = {{7, 64 - 9}, {9, 64 - 7}};

    for (int side = 0; side < 2; side++){
        // Gather opp king region
        int opp = side^1;
        U64 oppKing = board.bitboards[Board::KING | opp];
        int oppKingSq = numberOfTrailingZeros(oppKing);
        U64 oppKingRegion = BitBoardGen::BITBOARD_KING_REGION[opp][oppKingSq];

        // 2. Prepare X-Ray Occupancies
        // These bitboards remove OUR sliders from the occupancy.
        // This effectively makes our own pieces "transparent" to the attack generator.
        U64 myRooks   = board.bitboards[Board::ROOK | side];
        U64 myBishops = board.bitboards[Board::BISHOP | side];
        U64 myQueens  = board.bitboards[Board::QUEEN | side];

        // For Linear attacks (Rooks/Queens): Remove my Rooks and Queens
        U64 occupLinear   = occup ^ (myRooks | myQueens);

        // For Diagonal attacks (Bishops/Queens): Remove my Bishops and Queens
        U64 occupDiagonal = occup ^ (myBishops | myQueens);

        //rooks
        U64 rooks = myRooks;
        while (rooks){	
            int from = numberOfTrailingZeros(rooks);
            U64 tmpTarg = Magic::rookAttacksFrom(occupLinear, from);
            ei.attackInfo.rooks[side]|= tmpTarg;

            if (tmpTarg & oppKingRegion)
                ei.attackInfo.rookAttackersKing[side]++;

            rooks&= rooks - 1;
        }

        //bishops
        U64 bishops = myBishops;
        while (bishops){
            int from = numberOfTrailingZeros(bishops);
            U64 tmpTarg = Magic::bishopAttacksFrom(occupDiagonal, from);
            ei.attackInfo.bishops[side]|=  tmpTarg;

            if (tmpTarg & oppKingRegion)
                ei.attackInfo.bishopAttackersKing[side]++;

            bishops&= bishops - 1;
        }

        //queens
        U64 queens = myQueens;
        while (queens){	
            int from = numberOfTrailingZeros(queens);

            // Queens combine both X-Ray types
            U64 linear   = Magic::rookAttacksFrom(occupLinear, from);
            U64 diagonal = Magic::bishopAttacksFrom(occupDiagonal, from);
            U64 tmpTarg  = linear | diagonal;

            ei.attackInfo.queens[side]|= tmpTarg;

            if (tmpTarg & oppKingRegion)
                ei.attackInfo.queenAttackersKing[side]++;

            queens&= queens - 1;
        }

        //knights
        U64 knights = board.bitboards[Board::KNIGHT | side];        
        while (knights){
            int from = numberOfTrailingZeros(knights);
            U64 tmpTarg = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from];
            ei.attackInfo.knights[side]|=  tmpTarg;

            if (tmpTarg & oppKingRegion)
                ei.attackInfo.knightAttackersKing[side]++;

            knights&= knights - 1;
        }

        //Pawns
        U64 pawnBB = board.bitboards[Board::PAWN | side];
        int epSquare = board.state.epSquare;

        for (int i = 0; i < 2; i++){
            U64 wFile = BitBoardGen::WRAP_FILES[i];
            U64 attacks = BitBoardGen::circular_lsh(pawnBB, dirs[i][side]) & ~wFile;
            ei.attackInfo.pawns[side]|= attacks;
        }      
    }
}

void Evaluation::evalPawns(const Board& board, EvalInfo& ei, int& mg, int& eg){
	const int up_ahead[2] = {8, -8};	
	int s = 1;
	U64 occup = ei.occup;

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

			U64 stoppers = frontSpan & oppPawns;
			
			if (!stoppers){
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
						if (!(board.bitboards[side] & bb)){
                            U64 allAttacksSide = ei.attackInfo.rooks[side] | ei.attackInfo.knights[side] | ei.attackInfo.bishops[side] |
                                                 ei.attackInfo.queens[side] | ei.attackInfo.pawns[side];
							defendedSquares&= allAttacksSide;
                        }

						if (!(board.bitboards[opp] & bb)){
                            U64 allAttacksOpp = ei.attackInfo.rooks[opp] | ei.attackInfo.knights[opp] | ei.attackInfo.bishops[opp] |
                                                ei.attackInfo.queens[opp] | ei.attackInfo.pawns[opp];
							unsafeSquares&= allAttacksOpp | board.bitboards[opp];
                        }

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
		}
		s = -1;
	}
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

void Evaluation::evalKingAttack(const Board& board, int& mg, EvalInfo& ei){
    mg+= kingAttack(board, Board::WHITE, ei) - kingAttack(board, Board::BLACK, ei);
}

int Evaluation::kingAttack(const Board& board, int side, EvalInfo& ei){
    int opp = side^1;
    U64 king = board.bitboards[Board::KING | opp];
    int kingSq = numberOfTrailingZeros(king);
    U64 attackerPawns = board.bitboards[Board::PAWN | side];

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
    int nrooks = ei.attackInfo.rookAttackersKing[side];
    numAttackers+= nrooks;
    attackVal+= nrooks * ROOK_AW;
    
    //queens
    int nqueens = ei.attackInfo.queenAttackersKing[side];
    numAttackers+= nqueens;
    attackVal+= nqueens * QUEEN_AW;
    
    //bishops
    int nbishops = ei.attackInfo.bishopAttackersKing[side];
    numAttackers+= nbishops;
    attackVal+= nbishops * BISHOP_AW;
    
    //knights
    int nknights = ei.attackInfo.knightAttackersKing[side];
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

void Evaluation::outposts(const Board& board, EvalInfo& ei, int&mg, int&eg){
	int s = 1;
	const int min_rank[2] = {3, 1};
	const int max_rank[2] = {6, 4};

	for (int side = 0; side < 2; side++){
		int opp = side^1;

		// Knights
		U64 kn = board.bitboards[Board::KNIGHT | side];
        U64 ourPawnAttacks = ei.attackInfo.pawns[side];
		U64 enemyPawnAttacks = ei.attackInfo.pawns[opp];
        U64 enemyPawns = board.bitboards[Board::PAWN | opp];
        
		while (kn){
			int sq = numberOfTrailingZeros(kn);
            U64 sqBB = BitBoardGen::SQUARES[sq];
			int rank = sq >> 3;

			if (rank >= min_rank[side] && rank <= max_rank[side] && (ourPawnAttacks & sqBB)){
                if (!(enemyPawnAttacks & sqBB)){
                    int file = sq & 7;
					mg+= s * KNIGHT_OUTPOST_BONUS_MG[file];
					eg+= s * KNIGHT_OUTPOST_BONUS_EG[file];

                    // is it a hole?
                    if ((BitBoardGen::ADJACENT_FILES[file] & enemyPawns) == 0){
                        mg += s * KNIGHT_HOLE_BONUS_MG;
                        eg += s * KNIGHT_HOLE_BONUS_EG;
                    }
				}
			}			
			kn&= kn - 1;
		}

		// Bishops
		U64 bishops = board.bitboards[Board::BISHOP | side];
        
		while (bishops){
			int sq = numberOfTrailingZeros(bishops);
            U64 sqBB = BitBoardGen::SQUARES[sq];
			int rank = sq >> 3;

			if (rank >= min_rank[side] && rank <= max_rank[side] && (ourPawnAttacks & sqBB)){
				if (!(enemyPawnAttacks & sqBB)){
                    int file = sq & 7;
					mg+= s * BISHOP_OUTPOST_BONUS_MG[file];
					eg+= s * BISHOP_OUTPOST_BONUS_EG[file];

                    // is it a hole?
                    if ((BitBoardGen::ADJACENT_FILES[file] & enemyPawns) == 0){
                        mg += s * BISHOP_HOLE_BONUS_MG;
                        eg += s * BISHOP_HOLE_BONUS_EG;
                    }
				}
			}
			bishops&= bishops - 1;
		}
		s = -1;
	}
}

static const int king_shelter_bonus1 = 8;
static const int king_shelter_bonus2 = 4;
//one sq ahead region
static const int king_shelter_attacked_penalty1[] = {0, 0, -16, -48};
//two squares ahead region
static const int king_shelter_attacked_penalty2[] = {0, 0, -8, -24};
//opp connected pawns attacking shelter
static const int king_connected_attack_penalty = -5;

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
		
		s = -1;
	}
}

void Evaluation::mobility(const Board& board, EvalInfo& ei, int& mg, int& eg) {
    int s = 1;
    // Pre-calculated bonuses per piece per number of moves (0-27 for Queen)
    const int KNIGHT_MOBILITY_MG[9] = {-30, -15, 0, 5, 10, 15, 18, 20, 22};
    const int BISHOP_MOBILITY_MG[14] = {-20, -10, 0, 3, 6, 9, 12, 14, 16, 18, 20, 21, 22, 23};
    const int ROOK_MOBILITY_MG[15]   = {-12, -6, 0, 2, 4, 6, 8, 10, 11, 12, 13, 13, 14, 14, 14};
    const int QUEEN_MOBILITY_MG[28]  = {-10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 18};

    // const int KNIGHT_MOBILITY_EG[9] = {-20, -10, 0, 2, 4, 6, 8, 9, 10};
    // const int BISHOP_MOBILITY_EG[14]= {-10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10};
    // const int ROOK_MOBILITY_EG[15]  = {-10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 10};
    // const int QUEEN_MOBILITY_EG[28] = {-10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 18};
    int scale = 2;

    for (int side = 0; side < 2; ++side) {
        U64 ourPieces = board.bitboards[side];
        
        U64 knights = board.bitboards[Board::KNIGHT | side];
        while (knights) {
            int sq = numberOfTrailingZeros(knights);
            U64 moves = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[sq] & ~ourPieces;
            int count = BitBoardGen::popCount(moves);
            mg += s * KNIGHT_MOBILITY_MG[count]/scale;
            //eg += s * KNIGHT_MOBILITY_EG[count]/scale;
            knights &= knights - 1;
        }

        U64 bishops = board.bitboards[Board::BISHOP | side];
        while (bishops) {
            int sq = numberOfTrailingZeros(bishops);
            U64 moves = Magic::bishopAttacksFrom(ei.occup, sq) & ~ourPieces;
            int count = BitBoardGen::popCount(moves);
            mg += s * BISHOP_MOBILITY_MG[count]/scale;
            //eg += s * BISHOP_MOBILITY_EG[count]/scale;
            bishops &= bishops - 1;
        }
        
        U64 rooks = board.bitboards[Board::ROOK | side];
        while (rooks) {
            int sq = numberOfTrailingZeros(rooks);
            U64 moves = Magic::rookAttacksFrom(ei.occup, sq) & ~ourPieces;
            int count = BitBoardGen::popCount(moves);
            mg += s * ROOK_MOBILITY_MG[count]/scale;
            //eg += s * ROOK_MOBILITY_EG[count]/scale;
            rooks &= rooks - 1;
        }
        
        U64 queens = board.bitboards[Board::QUEEN | side];
        while(queens) {
            int sq = numberOfTrailingZeros(queens);
            U64 moves = (Magic::rookAttacksFrom(ei.occup, sq) | Magic::bishopAttacksFrom(ei.occup, sq)) & ~ourPieces;
            int count = BitBoardGen::popCount(moves);
            mg += s * QUEEN_MOBILITY_MG[count]/scale;
            //eg += s * QUEEN_MOBILITY_EG[count]/scale;
            queens &= queens - 1;
        }
        s = -1;
    }
}

void Evaluation::initEvalInfo(const Board& board, EvalInfo& ei){
    // inits occupancy and attacks BBs
    computeAttacks(board, ei);
}

int Evaluation::evaluate(const Board& board) {
    int mg = 0;
    int eg = 0;
    int phase = 0;

    // Initializes eval info, so we don't recompute stuff again
    EvalInfo ei;
    initEvalInfo(board, ei);

    materialBalance(board, mg, eg);
    pieceSquares(board, mg, eg, phase);
    evalPawns(board, ei, mg, eg);
    pieceOpenFile(board, mg, eg);
    evalKingAttack(board, mg, ei);
    outposts(board, ei, mg, eg);
    kingShelter(board, mg);
    mobility(board, ei, mg, eg);

    if (phase > TOTAL_PHASE) phase = TOTAL_PHASE;
    int score = ((mg * phase) + (eg * (TOTAL_PHASE - phase))) / TOTAL_PHASE;
    return (board.state.currentPlayer == Board::WHITE) ? score : -score;
}

/* Eval mirror testing */
Board Evaluation::mirrorBoard(Board& board) {
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

    int mPieces[] = { 0, 0, Board::BLACK_PAWN, Board::WHITE_PAWN, Board::BLACK_KNIGHT, Board::WHITE_KNIGHT,
        Board::BLACK_BISHOP, Board::WHITE_BISHOP, Board::BLACK_ROOK, Board::WHITE_ROOK,
        Board::BLACK_QUEEN, Board::WHITE_QUEEN, Board::BLACK_KING, Board::WHITE_KING };

    //mirror pieces
    for (int i = 0; i < 64; i++)
        mBoard.board[i] = mPieces[board.board[Evaluation::MIRROR64[i]]];

    //setup bitboards
    for (int s = 0; s < 64; s++) {
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
    mBoard.state = BoardState(tmpEP, board.state.halfMoves, side ^ 1, tmpCastle, Zobrist::getKey(mBoard));
    mBoard.ply = 0;
    mBoard.hashTable = NULL;

    return mBoard;
}

void Evaluation::testEval(std::string test_file) {
    std::ifstream file(test_file);
    std::string line;
    int n = 0;

    printf("Running eval test\n");

    while (std::getline(file, line)) {
        Board board = FenParser::parseFEN(line);
        Board mBoard = mirrorBoard(board);
        int ev1 = evaluate(board);
        int ev2 = evaluate(mBoard);

        if (ev1 != ev2) {
            std::cout << "Eval test fail at FEN: " << line << std::endl;
            return;
        }
        n++;
    }
    printf("Evaluation ok: tested %d positions.\n", n);
}