#include "Evaluation.h"
#include "MoveGen.h"
#include "FenParser.h"
#include "Zobrist.h"
#include <iostream>

#define ROOK_OPEN_FILE_BONUS 14
#define ROOK_SEMIOPEN_FILE_BONUS 7
#define QUEEN_OPEN_FILE_BONUS 7
#define QUEEN_SEMIOPEN_FILE_BONUS 3

#define PAWN_SQ {0, 0, 0, 0,  0,  0,  0,  0, 5, 10, 10,-20,-20, 10, 10,  5, 5, -5,-10,  0,  0,-10, -5,  5, 0,  0,  0, 20, 20,  0,  0,  0, 5,  5, 10, 25, 25, 10,  5,  5, 10, 10, 20, 30, 30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50, 0,  0,  0,  0,  0,  0,  0,  0}						
#define KNIGHT_SQ {-50,-40,-30,-30,-30,-30,-40,-50,-40, -20,  0,  5,  5,  0, -20, -40,-30,  5, 10, 15, 15, 10,  5, -30, -30,  0, 15, 20, 20, 15,  0,-30,-30,  5, 15, 20, 20, 15,  5,-30,-30,  0, 10, 15, 15, 10,  0, -30, -40, -20,  0,  0,  0,  0,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50}							
#define BISHOP_SQ {-20,-10,-10,-10,-10,-10,-10,-20,-10,  5,  0,  0,  0,  0,  5,-10,-10, 10, 10, 10, 10, 10, 10,-10,-10,  0, 10, 10, 10, 10,  0,-10,-10,  5,  5, 10, 10,  5,  5,-10,-10,  0,  5, 10, 10,  5,  0,-10,-10,  0,  0,  0,  0,  0,  0,-10,-20,-10,-10,-10,-10,-10,-10,-20}							  
#define ROOK_SQ {-5, 0, 5, 10, 10, 5, 0, -5, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0, 0, 0,  0,  5,  10,  10,  5,  0,  0, 20, 20, 20, 20, 20, 20, 20, 20, 0,  0,  0,  0,  0,  0,  0,  0}
#define QUEEN_SQ {-20,-10,-10, -5, -5,-10,-10,-20,-10,  0,  5,  0,  0,  0,  0,-10,-10,  5,  5,  5,  5,  5,  0,-10,0,   0,  5,  5,  5,  5,  0, -5,-5,   0,  5,  5,  5,  5,  0, -5,-10,  0,  5,  5,  5,  5,  0,-10,-10,  0,  0,  0,  0,  0,  0,-10,-20,-10,-10, -5, -5,-10,-10,-20}							
#define KING_SQ {20, 30, 10,  0,  0, 10, 30, 20,20, 20,  0,  0,  0,  0, 20, 20,-10,-20,-20,-20,-20,-20,-20,-10,-20,-30,-30,-40,-40,-30,-30,-20,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30}							 
#define KING_END_SQ {-50,	-40	,	0	,	0	,	0	,	0	,	-40	,	-50	, -20,	0	,	10	,	15	,	15	,	10	,	0	,	-20	, 0	,	10	,	20	,	20	,	20	,	20	,	10	,	0	, 0	,	10	,	30	,	40	,	40	,	30	,	10	,	0	, 0	,	10	,	20	,	40	,	40	,	20	,	10	,	0	, 0	,	10	,	20	,	20	,	20	,	20	,	10	,	0	, -10,	0	,	10	,	10	,	10	,	10	,	0	,	-10	, -50	,	-10	,	0	,	0	,	0	,	0	,	-10	,	-50}

int Evaluation::PIECE_VALUES[14] = {0, 0, PAWN_VAL, -PAWN_VAL, KNIGHT_VAL, -KNIGHT_VAL, BISHOP_VAL,
					-BISHOP_VAL, ROOK_VAL, -ROOK_VAL, QUEEN_VAL, -QUEEN_VAL, KING_VAL, -KING_VAL};
					
int Evaluation::PIECE_SQUARES[14][64] = {{}, {}, 
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


//file based
int Evaluation::ISOLATED_PAWN_PENALTY[8] = {-15, -10, -10, -10, -10, -10, -10, -15};
//rank based
int Evaluation::PASSED_PAWN_BONUS[2][8] = {{0, 5, 10, 20, 40, 90, 150, 200}, {200, 150, 90, 40, 20, 10, 5, 0}};
int Evaluation::MIRROR64[64];

void Evaluation::initAll(){
	for (int i = 0; i < 64; i++){
		int r = i/8;
		int c = i % 8;
		int sqBlack = (7 - r)* 8 + c;
		MIRROR64[i] = sqBlack;
	}
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

int Evaluation::materialBalance(Board& board){
	int s = 0;
	for (int i = 0; i < 64; i++){
		s+= PIECE_VALUES[board.board[i]];
	}
	return s;
}

int Evaluation::pieceSquaresBalance(Board& board){
	int s = 0;
	for (int i = 0; i < 64; i++){
		if (board.board[i] == Board::EMPTY)
			continue;
		int *pieceSquare = PIECE_SQUARES[board.board[i]];
		if ((BitBoardGen::SQUARES[i] & board.bitboards[Board::WHITE]) != 0)
			s+= pieceSquare[i];
		else
			s-= pieceSquare[MIRROR64[i]];
	}
	return s;
}		

int Evaluation::pieceSquaresBalanceEnd(Board& board){
	int s = 0;
	for (int i = 0; i < 64; i++){
		if (board.board[i] == Board::EMPTY)
			continue;

		int *pieceSquare = PIECE_SQUARES_END[board.board[i]];
		//if ((BitBoardGen::SQUARES[i] & board.bitboards[Board::WHITE]) != 0)
		if ((board.board[i] & 1) == Board::WHITE)
			s+= pieceSquare[i];
		else
			s-= pieceSquare[MIRROR64[i]];
	}
	return s;
}

int Evaluation::kingAttacked(Board& board, int side){
	int opp = side ^ 1;
	U64 king = board.bitboards[Board::KING | side];
	int kingSq = numberOfTrailingZeros(king);
	U64 region = BitBoardGen::BITBOARD_KING_REGION[side][kingSq];
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 enemyOrEmpty = ~board.bitboards[opp]; //include opp king square
	
	int numAttackers = 0;
	int attackVal = 0;
	//Attack weights
	int ROOK_AW = 40;
	int QUEEN_AW = 80;
	int BISHOP_AW = 20;
	int KNIGHT_AW = 20;
	//Weight of attack by numAttackers, 0 or 1 attacker => 0 weight
	int ATTACK_W[] = {0, 0, 50, 75, 88, 94, 97, 99};

	//rooks
	U64 rooks = board.bitboards[Board::ROOK | opp];
	U64 rookTargs = 0;
	U64 *up = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP];
	U64 *right = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_RIGHT];
	U64 *down = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN];
	U64 *left = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_LEFT];
	int nrooks = 0;
	
	while (rooks != 0){	
		int from = numberOfTrailingZeros(rooks);
		U64 upward =  MoveGen::upwardMoveTargetsFrom(from, occup, up, enemyOrEmpty) | MoveGen::upwardMoveTargetsFrom(from, occup, right, enemyOrEmpty);
		U64 downward =  MoveGen::downwardMoveTargetsFrom(from, occup, down, enemyOrEmpty) | MoveGen::downwardMoveTargetsFrom(from, occup, left, enemyOrEmpty);
		U64 tmpTarg = upward | downward;
		rookTargs |=  tmpTarg;

		if ((tmpTarg & region) != 0)	
			nrooks++;
		rooks &= rooks - 1;
	}
	numAttackers += nrooks;
	attackVal += nrooks * ROOK_AW;
	
	//queens
	U64 queens = board.bitboards[Board::QUEEN | opp];
	U64 queenTargs = 0;
	U64 *up_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_LEFT];
	U64 *up_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_UP_RIGHT];
	U64 *down_l = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_LEFT];
	U64 *down_r = BitBoardGen::BITBOARD_DIRECTIONS[BitBoardGen::IDX_DOWN_RIGHT];
	int nqueens = 0;
	
	while (queens != 0){	
		int from = numberOfTrailingZeros(queens);
		U64 upward =  MoveGen::upwardMoveTargetsFrom(from, occup, up_r, enemyOrEmpty) | MoveGen::upwardMoveTargetsFrom(from, occup, up_l, enemyOrEmpty)
							  | MoveGen::upwardMoveTargetsFrom(from, occup, up, enemyOrEmpty) | MoveGen::upwardMoveTargetsFrom(from, occup, right, enemyOrEmpty);
		U64 downward =  MoveGen::downwardMoveTargetsFrom(from, occup, down_r, enemyOrEmpty) | MoveGen::downwardMoveTargetsFrom(from, occup, down_l, enemyOrEmpty)
							 | MoveGen::downwardMoveTargetsFrom(from, occup, down, enemyOrEmpty) | MoveGen::downwardMoveTargetsFrom(from, occup, left, enemyOrEmpty);
		U64 tmpTarg = upward | downward;
		queenTargs |=  tmpTarg;

		if ((tmpTarg & region) != 0)	
			nqueens++;
		queens &= queens - 1;
	}
	
	numAttackers += nqueens;
	attackVal += nqueens * QUEEN_AW;
	
	//bishops
	U64 bishops = board.bitboards[Board::BISHOP | opp];
	U64 bishopTargs = 0;
	int nbishops = 0;
	
	while (bishops != 0){
		int from = numberOfTrailingZeros(bishops);
		U64 upward =  MoveGen::upwardMoveTargetsFrom(from, occup, up_r, enemyOrEmpty) | MoveGen::upwardMoveTargetsFrom(from, occup, up_l, enemyOrEmpty);
		U64 downward =  MoveGen::downwardMoveTargetsFrom(from, occup, down_r, enemyOrEmpty) | MoveGen::downwardMoveTargetsFrom(from, occup, down_l, enemyOrEmpty);
		U64 tmpTarg = upward | downward;
		bishopTargs |=  tmpTarg;

		if ((tmpTarg & region) != 0)
			nbishops++;
		bishops &= bishops - 1;
	}
	
	numAttackers += nbishops;
	attackVal += nbishops * BISHOP_AW;
	
	//knights
	long knights = board.bitboards[Board::KNIGHT | opp];
	long knightTargs = 0;
	int nknights = 0;
	
	while (knights != 0){
		int from = numberOfTrailingZeros(knights);
		long tmpTarg = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from] & enemyOrEmpty;
		knightTargs |= tmpTarg;

		if ((tmpTarg & region) != 0)
			nknights++;
		knights &= knights - 1;
	}
	
	numAttackers += nknights;
	attackVal += nknights * KNIGHT_AW;
	
	//ATTACK_W.length -> 8
	//numAttackers = numAttackers >= ATTACK_W.length ? ATTACK_W.length - 1 : numAttackers;
	numAttackers = numAttackers >= 8 ? 7 : numAttackers;
	attackVal = attackVal * ATTACK_W[numAttackers];
	return attackVal/100;
}													

int Evaluation::isolatedPawnsSide(Board& board, int side){
	int pawnSide = Board::PAWN | side;
	U64 pawnBB = board.bitboards[pawnSide];
	U64 pawns = board.bitboards[pawnSide];
	int penalty = 0;
	
	while (pawns != 0){
		int sq = numberOfTrailingZeros(pawns);
		int file = sq & 7;
		U64 adj_files = BitBoardGen::ADJACENT_FILES[file];
		
		if ((adj_files & pawnBB) == 0){
			penalty += ISOLATED_PAWN_PENALTY[file];
		}
		pawns&= pawns - 1;
	}
	return penalty;
}

//connected passed bonus
static int conn_bonus[9] = {0, 0, 20, 40, 80, 100, 120, 140, 160};
int Evaluation::passedPawnsSide(Board& board, int side){
	U64 pawns = board.bitboards[Board::PAWN | side];
	U64 oppPawns = board.bitboards[side^1 | Board::PAWN];
	int pp = 0;
	int conn_file[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	while (pawns != 0){
		int sq = numberOfTrailingZeros(pawns);
		U64 frontSpan = BitBoardGen::FRONT_SPAN[side][sq];
		
		if ((frontSpan & oppPawns) == 0){
			pp += PASSED_PAWN_BONUS[side][sq >> 3];
			conn_file[sq & 7] = 1;
		}
		pawns&= pawns - 1;
	}

	//apply connected bonus
	int cnt = 0;
	for (int i = 0; i < 8; ++i){
		if (conn_file[i] == 0){
			pp+= conn_bonus[cnt];
			cnt = 0;
		}else{
			cnt++;
		}
	}
	pp+= conn_bonus[cnt];
	return pp;
}

int Evaluation::pieceOpenFileSide(Board& board, int side, int pieceType, int bonusOpen, int bonusSemiOpen){
	int opp = side^1;
	U64 pieces = board.bitboards[pieceType| side];
	U64 pawnSide = board.bitboards[Board::PAWN | side];
	U64 pawnOpp = board.bitboards[Board::PAWN | opp];
	U64 pawnBoth = pawnSide | pawnOpp;
	int score = 0;

	while(pieces){
		int sq = numberOfTrailingZeros(pieces);
		int file = sq & 7;

		if ((pawnBoth & BitBoardGen::BITBOARD_FILES[file]) == 0){
			score += bonusOpen;
		}
		else if ((pawnSide & BitBoardGen::BITBOARD_FILES[file]) == 0){
			score+= bonusSemiOpen;
		}
		pieces&= pieces - 1;
	}
	return score;
}

static int king_shelter_bonus1 = 8;
static int king_shelter_bonus2 = 4;
static int king_shelter_attack_bonus1[] = {-8, -16, -48};
static int king_shelter_attack_bonus2[] = {-4, -8, -24};
static int both_king_zones_enemy_pawn_bonus = -20;
int Evaluation::kingShelterSide(Board& board, int side){
	int opp = side^1;
	int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);
	U64 front1 = BitBoardGen::BITBOARD_KING_AHEAD[side][ks][0];
	U64 front2 = BitBoardGen::BITBOARD_KING_AHEAD[side][ks][1];
	U64 myPawns = board.bitboards[Board::PAWN | side];
	U64 oppPawns = board.bitboards[Board::PAWN | opp];
	
	//defense pawns
	U64 shelter = front1 & myPawns;
	int bonus = 0;
	while(shelter != 0){
		bonus+= king_shelter_bonus1;
		shelter&= shelter - 1;
	}
	
	shelter = front2 & myPawns;
	while(shelter != 0){
		bonus+= king_shelter_bonus2;
		shelter&= shelter - 1;
	}
	
	//enemy pawns
	shelter = front1 & oppPawns;
	int cnt1 = 0;
	while(shelter != 0){
		bonus+= king_shelter_attack_bonus1[cnt1];
		shelter&= shelter - 1;
		cnt1++;
	}
	
	shelter = front2 & oppPawns;
	int cnt2 = 0;
	while(shelter != 0){
		bonus+= king_shelter_attack_bonus2[cnt2];
		shelter&= shelter - 1;
		cnt2++;
	}

	//enemy pawn on both king fronts
	if (cnt1 && cnt2){
		bonus+= both_king_zones_enemy_pawn_bonus;
	}
	return bonus;
}


int Evaluation::kingDistToEnemyPawnsSide(Board& board, int side){

	int d = 0;
	int opp = side^1;
	U64 pawns = board.bitboards[Board::PAWN | opp];
	int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);

	while(pawns != 0){
		int sq = numberOfTrailingZeros(pawns);
		d+= BitBoardGen::DISTANCE_SQS[sq][ks];
		pawns&= pawns - 1;
	}
	return -d;
}

int Evaluation::kingDistToEnemyPawns(Board& board){
	return kingDistToEnemyPawnsSide(board, Board::WHITE) - kingDistToEnemyPawnsSide(board, Board::BLACK);
}

int Evaluation::isolatedPawns(Board& board){
	return isolatedPawnsSide(board, Board::WHITE) - isolatedPawnsSide(board, Board::BLACK);
}

int Evaluation::passedPawns(Board& board){
	return passedPawnsSide(board, Board::WHITE) - passedPawnsSide(board, Board::BLACK);
}

int Evaluation::pieceOpenFile(Board& board){ 
	int score = 0;

	score+= pieceOpenFileSide(board, Board::WHITE, Board::ROOK, ROOK_OPEN_FILE_BONUS, ROOK_SEMIOPEN_FILE_BONUS)-
			pieceOpenFileSide(board, Board::BLACK, Board::ROOK, ROOK_OPEN_FILE_BONUS, ROOK_SEMIOPEN_FILE_BONUS);

	score+= pieceOpenFileSide(board, Board::WHITE, Board::QUEEN, QUEEN_OPEN_FILE_BONUS, QUEEN_SEMIOPEN_FILE_BONUS)-
			pieceOpenFileSide(board, Board::BLACK, Board::QUEEN, QUEEN_OPEN_FILE_BONUS, QUEEN_SEMIOPEN_FILE_BONUS);
	return score;
}

//Mobility
static int MOB_N[2][9] = {{-75, -56, -9, -2, 6, 15, 22, 30, 36}, {-76, -54, -26, -10, 5, 11, 26, 28, 29}};
static int MOB_B[2][14] = {{-48, -21 ,16, 26, 37, 51, 54, 63, 65, 71, 79, 81, 92, 97}, {-58, -19, -2, 12, 22, 42, 54, 58, 63, 70, 74, 86, 90, 94}};
static int MOB_R[2][15] = {{-56, -25, -11, -5, -4, -1, 8, 14, 21, 23, 31, 32, 43, 49, 59}, {-78, -18, 26, 55, 70, 81, 109, 120, 128, 143, 154, 160, 165, 168, 169}};
static int MOB_Q[2][28] = {{-40, -25, 2, 4, 14, 24, 25, 40, 43, 47, 54, 56, 60, 70, 72, 73, 75, 
					77, 85, 94, 99, 108, 112, 113, 118, 119, 123, 128}, 
					{-35, -12, 7, 19, 37, 55, 62, 76, 79, 87, 94, 102, 111, 116, 118, 122,
					128, 130, 133, 136, 140, 157, 158, 161, 174, 177, 191, 199}};


//knight & bishop -> not attacked by opp pawns
//rook -> not attacked by opp pawn minor
//queen -> not attacked by opp pawn minor or rook
//opp king included at attack square
std::pair<int, int> Evaluation::mobility(Board& board, int side){

	//Define this outside =p
	int dirs[2][2] = {{7, 64 - 9}, {9, 64 - 7}};
	int diffs[2][2] = {{7, -9}, {9, -7}};
	
	int opp = side^1;
	U64 oppPawnBB = board.bitboards[Board::PAWN | opp];
	U64 oppPawnAttacks = 0;
	
	if (oppPawnBB){
		for (int i = 0; i < 2; i++){
			int *dir = dirs[i];
			int *diff = diffs[i];
			U64 wFile = BitBoardGen::WRAP_FILES[i];
			oppPawnAttacks|= BitBoardGen::circular_lsh(oppPawnBB, dir[opp]) & ~wFile;
		}
	}

	U64 enemyOrEmpty = ~board.bitboards[side] & ~board.bitboards[Board::KING | opp];
	U64 notOppPawnAttacks = ~oppPawnAttacks;
	
	//Knight
	U64 safeKN = 0;
	U64 kns = board.bitboards[Board::KNIGHT | side];
	int mobKN = 0;
	while (kns != 0){
		int from = numberOfTrailingZeros(kns);
		U64 targets = BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from] & notOppPawnAttacks & enemyOrEmpty;
		safeKN |= targets;
		mobKN+= BitBoardGen::popCount(targets);
		kns &= kns - 1;
	}
	
	//DEBUG
	//printf("Save KN mob\n");
	//BitBoardGen::printBB(safeKN);
	
	//Bishop
	U64 safeBish = 0;
	U64 occup = board.bitboards[side] | board.bitboards[opp];
	U64 bishops = board.bitboards[Board::BISHOP | side];
	int mobBS = 0;
	while (bishops != 0){
		int from = numberOfTrailingZeros(bishops);
		U64 targets = MoveGen::bishopAttacks(board, occup, from, side) & notOppPawnAttacks & enemyOrEmpty;
		safeBish |= targets;
		mobBS+= BitBoardGen::popCount(targets);
		bishops&= bishops - 1;
	}
	
	//DEBUG
	//printf("Save BSHP mob\n");
	//BitBoardGen::printBB(safeBish);
	
	//Rook
	//First get opp Knight and Bishop attacks
	U64 oppKN = board.bitboards[Board::KNIGHT | opp];
	U64 oppKNAttacks = 0;
	while(oppKN){
		int from = numberOfTrailingZeros(oppKN);
		oppKNAttacks|= BitBoardGen::BITBOARD_KNIGHT_ATTACKS[from];
		oppKN &= oppKN - 1;
	}
	
	U64 oppBishops = board.bitboards[Board::BISHOP | opp];
	U64 oppBishopAttacks = 0;
	while(oppBishops){
		int from = numberOfTrailingZeros(oppBishops);
		oppBishopAttacks|= MoveGen::bishopAttacks(board, occup, from, opp);
		oppBishops &= oppBishops - 1;
	}
	
	U64 notOppMinorAttacks = notOppPawnAttacks & ~(oppBishopAttacks | oppKNAttacks);
	U64 rooks = board.bitboards[Board::ROOK | side];
	int mobRK = 0;
	U64 safeRook = 0;
	while(rooks){
		int from = numberOfTrailingZeros(rooks);
		U64 targets = MoveGen::rookAttacks(board, occup, from, side) & notOppMinorAttacks & enemyOrEmpty;
		safeRook |= targets;
		mobRK+= BitBoardGen::popCount(targets);
		rooks&= rooks - 1;
	}
	
	//DEBUG
	//printf("Save ROOK mob\n");
	//BitBoardGen::printBB(safeRook);
	
	//Queen
	//Need to exclude opp Rook attacks
	U64 oppRookAttacks = 0;
	U64 oppRooks = board.bitboards[Board::ROOK | opp];
	while(oppRooks){
		int from = numberOfTrailingZeros(oppRooks);
		oppRookAttacks|= MoveGen::rookAttacks(board, occup, from, opp);
		oppRooks&= oppRooks - 1;
	}
	
	notOppMinorAttacks&= ~oppRookAttacks;
	U64 queens = board.bitboards[Board::QUEEN | side];
	U64 safeQ = 0;
	int mobQ = 0;
	while(queens){
		int from = numberOfTrailingZeros(queens);
		U64 rookAtt = MoveGen::rookAttacks(board, occup, from, side);
		U64 bishopAtt = MoveGen::bishopAttacks(board, occup, from, side);
		U64 targets = (rookAtt | bishopAtt) & notOppMinorAttacks & enemyOrEmpty;
		safeQ |= targets;
		mobQ+= BitBoardGen::popCount(targets);
		queens&= queens - 1;
	}
	
	//DEBUG
	//printf("Save QUEEN mob\n");
	//BitBoardGen::printBB(safeQ);
	
	int mobBonusOpen = MOB_N[0][mobKN] + MOB_B[0][mobBS] + MOB_Q[0][mobQ] + MOB_R[0][mobRK];
	int mobBonusEnd = MOB_N[1][mobKN] + MOB_B[1][mobBS] + MOB_Q[1][mobQ] + MOB_R[1][mobRK];
	//p.first, p.second
	return std::make_pair(mobBonusOpen, mobBonusEnd);
}

Board Evaluation::mirrorBoard(Board& board){
	Board mBoard;
	int tmpCastle = 0;
	int tmpEP = 0;

	//Castle
	if (BoardState::white_can_castle_ks(board.state)) tmpCastle |= BoardState::BK_CASTLE;
	if (BoardState::white_can_castle_qs(board.state)) tmpCastle |= BoardState::BQ_CASTLE;
	if (BoardState::black_can_castle_ks(board.state)) tmpCastle |= BoardState::WK_CASTLE;
	if (BoardState::black_can_castle_qs(board.state)) tmpCastle |= BoardState::WQ_CASTLE;

	//Ep square
	if (BoardState::epSquare(board.state) != 0)
		tmpEP = Evaluation::MIRROR64[BoardState::epSquare(board.state)];

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

	int side = BoardState::currentPlayer(board.state);
	mBoard.histPly = board.histPly;
	mBoard.state = BoardState::new_state(tmpEP, BoardState::halfMoves(board.state), side ^ 1, tmpCastle);
	mBoard.zKey = Zobrist::getKey(mBoard);
	mBoard.ply = 0;
	mBoard.hashTable = NULL;

	return mBoard;
}

int Evaluation::materialValueSide(Board& board, int side){
	int mat = 0;

	for (int i = 0; i < 64; i++){
		if (board.board[i] == Board::EMPTY)
			continue;
		if ((board.board[i] & 1) == side)
			mat += PIECE_VALUES[board.board[i]];
	}
	return abs(mat) - KING_VAL;
}

int Evaluation::countMaterial(Board& board, int piece){
	
	U64 bb = board.bitboards[piece];
	int cnt = 0;
	
	while(bb != 0){
		cnt++;
		bb&= bb - 1;
	}
	return cnt;
}

//Phase
static int pawn_phase = 0;
static int knight_phase = 1;
static int bishop_phase = 1;
static int rook_phase = 2;
static int queen_phase = 4;
static int total_phase = 24;

int Evaluation::get_phase(Board& board){
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

//r1b1k2r/pp1p1pp1/4p2p/1P5q/1Qn1NP1P/4BP2/P3K1P1/R4B1R b kq - 0 17    move d7d5 kn capture ep!!
//ELO diff formula -400*log(1/p - 1)/log(10), p = win rate (with draws)
//5R2/8/8/3BK2p/7k/6p1/8/8 w - - 2 62 score = MATE
//r2q1rk1/pppb1ppp/3p1nn1/3Pp3/2P1P3/2B2N1P/PPQ2PP1/3RKB1R w K - 3 12 played e1e2??
int Evaluation::evaluate(Board& board, int side){

	if (materialDraw(board))
		return 0;

	int opp = side^1;
	//opening
	int mat = materialBalance(board);
	int pieceVal = pieceSquaresBalance(board);
	int isoPawns = isolatedPawns(board);
	int passPawns = passedPawns(board);
	int heavyOpen = pieceOpenFile(board);
	int kingSafe = kingAttacked(board, Board::BLACK) - kingAttacked(board, Board::WHITE);
	int kingShelter = kingShelterSide(board, Board::WHITE) - kingShelterSide(board, Board::BLACK);
	/*
	std::pair<int, int> mob_w = mobility(board, Board::WHITE);
	std::pair<int, int> mob_b = mobility(board, Board::BLACK);
	int mob_open = mob_w.first - mob_b.first;
	int mob_end = mob_w.second - mob_b.second;
	*/

	//endgame
	int pieceValEnd = pieceSquaresBalanceEnd(board);
	//int pawnDist = kingDistToEnemyPawns(board);

	int phase = get_phase(board);
	int opening = mat + pieceVal + isoPawns + passPawns + heavyOpen + kingShelter + kingSafe;// + mob_open;
	int endgame = mat + pieceValEnd + isoPawns + passPawns;// + mob_end;

	int eval = ((opening * (256 - phase)) + (endgame * phase))/256;
	eval = side == Board::WHITE ? eval : -eval;

	return eval;
}

#include <fstream>
void Evaluation::testEval(std::string test_file){
	std::ifstream file(test_file);
    std::string line; 
	int n = 0;

	printf("Running eval test\n");

    while (std::getline(file, line)){
    	Board board = FenParser::parseFEN(line);
    	Board mBoard = mirrorBoard(board);
    	bool eq = evaluate(board, BoardState::currentPlayer(board.state)) == evaluate(mBoard, BoardState::currentPlayer(mBoard.state));
    	n++;
    	if (!eq){
    		std::cout << "Eval test fail at FEN: " << line << std::endl;
    		return;
    	}
    }
    printf("Evaluation ok: tested %d positions.\n", n);
}
/*
int main(){
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Evaluation::initAll();
	Board board = FenParser::parseFEN("2kr1b2/1p3pr1/2p5/1p4n1/3N1Q1p/8/2P2P1n/2KR1B2 w - -");
	board.print();
	std::pair<int,int> m0 = Evaluation::mobility(board, 0);
	std::cout << "Black:" << std::endl;
	std::pair<int, int> m1 = Evaluation::mobility(board, 1);
	printf("White mobility: %d\nBlack mobility: %d\n", m0.first, m1.first);
	return 0;
}
*/