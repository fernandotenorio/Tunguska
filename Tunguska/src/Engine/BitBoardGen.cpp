#include "Engine/BitBoardGen.h"
#include "Engine/Board.h"
#include <stdio.h>

static void emptyBitString(char b[8][8]){
	for (int i = 0; i < 8; i++){
		for (int j = 0; j < 8; j++){
			b[i][j] = '0';
		}
	}
}

static U64 bitboardFromBitString(char bitString[8][8]){
	char lineBits[65];
	lineBits[64] = '\0';
	
	for (int s = 0; s < 64; s++){
		int sq = 63 - s;
		lineBits[s] = bitString[sq/8][sq % 8];
	}
	return fromBit(lineBits);
}

static U64 fromBit(char *p){
    U64 value = 0;
    while (*p) {
        value *= 2;
        value += *p - '0';
        p++;
    }
    return value;
}

int BitBoardGen::DIRECTIONS[8][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0},
							  {1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
							 
int BitBoardGen::KNIGHT_DIRECTIONS[8][2] = {{2, 1}, {2, -1}, {1, 2}, {1, -2}, 
							   {-2, 1}, {-2, -1}, {-1, 2}, {-1, -2}};

U64 BitBoardGen::setBit(U64 bb, int idx){
	return bb | (BitBoardGen::ONE << idx);
}

U64 BitBoardGen::zeroBit(U64 bb, int idx){
	return bb & ~(BitBoardGen::ONE << idx);
}

U64 BitBoardGen::circular_lsh(U64 target, int shift){
	return target << shift | target >> (BitBoardGen::SIX_FOUR - shift);
}

void BitBoardGen::printBB(U64 bb){
	for (int i = 7; i >= 0; i--){
		for (int j = 0; j < 8; j++){
			int idx = i * 8 + j;
		
			if ((BitBoardGen::ONE << idx & bb) != 0)
				printf("1 ");
			else
				printf("0 ");
		}
			printf("\n");
	}
	printf("\n");
}

U64 BitBoardGen::QS_CASTLE_OCCUP[2];
U64 BitBoardGen::KS_CASTLE_OCCUP[2];
void BitBoardGen::generateCastleMask(){
	char bitString[8][8];
	emptyBitString(bitString);
	
	bitString[0][Board::B1] = '1';
	bitString[0][Board::C1] = '1';
	bitString[0][Board::D1] = '1';
	BitBoardGen::QS_CASTLE_OCCUP[0] = bitboardFromBitString(bitString);
	
	emptyBitString(bitString);
	bitString[7][Board::B8 % 8] = '1';
	bitString[7][Board::C8 % 8] = '1';
	bitString[7][Board::D8 % 8] = '1';
	BitBoardGen::QS_CASTLE_OCCUP[1] = bitboardFromBitString(bitString);
	
	//ks
	emptyBitString(bitString);
	bitString[0][Board::F1] = '1';
	bitString[0][Board::G1] = '1';
	BitBoardGen::KS_CASTLE_OCCUP[0] = bitboardFromBitString(bitString);
	
	emptyBitString(bitString);
	bitString[7][Board::F8 % 8] = '1';
	bitString[7][Board::G8 % 8] = '1';
	BitBoardGen::KS_CASTLE_OCCUP[1] = bitboardFromBitString(bitString);
}

U64 BitBoardGen::BITBOARD_RANKS[8];
U64 BitBoardGen::RANKS_4_5_6_7[2];
void BitBoardGen::generateRanks(){
	char bitString[8][8];
	
	for (int r = 0; r < 8; r++){
		emptyBitString(bitString);
		
		for (int f = 0; f < 8; f++){
			bitString[r][f] = '1';
		}
		BitBoardGen::BITBOARD_RANKS[r] = bitboardFromBitString(bitString);
	}
	RANKS_4_5_6_7[0] = BITBOARD_RANKS[3] | BITBOARD_RANKS[4] | BITBOARD_RANKS[5] | BITBOARD_RANKS[6];
	RANKS_4_5_6_7[1] = BITBOARD_RANKS[1] | BITBOARD_RANKS[2] | BITBOARD_RANKS[3] | BITBOARD_RANKS[4];
}

U64 BitBoardGen::BITBOARD_FILES[8];
U64 BitBoardGen::CENTER_FILES;
void BitBoardGen::generateFiles(){
	char bitString[8][8];
	
	for (int f = 0; f < 8; f++){
		emptyBitString(bitString);
		
		for (int r = 0; r < 8; r++){
			bitString[r][f] = '1';
		}
		BitBoardGen::BITBOARD_FILES[f] = bitboardFromBitString(bitString);
	}
	CENTER_FILES = BITBOARD_FILES[2] | BITBOARD_FILES[3] | BITBOARD_FILES[4] | BITBOARD_FILES[5];
}

U64 BitBoardGen::WRAP_FILES[2];
void BitBoardGen::generateWrapFiles(){
	WRAP_FILES[0] = BITBOARD_FILES[7];
	WRAP_FILES[1] = BITBOARD_FILES[0];
}


U64 BitBoardGen::BITBOARD_KNIGHT_ATTACKS[64];
void BitBoardGen::generateKnight(){
	char bitString[8][8];
	
	for (int r = 0; r < 8; r++){
		for (int f = 0; f < 8; f++){
			emptyBitString(bitString);
			
			for (int k = 0; k < 8; k++){
				int *direction = BitBoardGen::KNIGHT_DIRECTIONS[k];
				int rr = r + direction[0];
				int ff = f + direction[1];
				
				if (rr >= 0 && rr < 8 && ff >= 0 && ff < 8)
					bitString[rr][ff] = '1';
			}
			BitBoardGen::BITBOARD_KNIGHT_ATTACKS[r * 8 + f] = bitboardFromBitString(bitString);
		}
	}
}

U64 BitBoardGen::BITBOARD_KING_AHEAD[2][64][2];
void BitBoardGen::generateKingAhead(){
	char bitString[8][8];
	int king_ahead1[] = {1, -1};
	int king_ahead2[] = {2, -2};
	
	for (int side = 0; side < 2; side++){
		for (int r = 0; r < 8; r++){
			for (int f = 0; f < 8; f++){
				emptyBitString(bitString);
				int left = f - 1;
				int right = f + 1;
				int up1 = r + king_ahead1[side];
				int up2 = r + king_ahead2[side];
				
				if (up1 >= 0 && up1 < 8){
					if (left >= 0 && left < 8)
						bitString[up1][left] = '1';
					if (right >= 0 && right < 8)
						bitString[up1][right] = '1';
					
					bitString[up1][f] = '1';
				}
				BitBoardGen::BITBOARD_KING_AHEAD[side][r * 8 + f][0] = bitboardFromBitString(bitString);
				emptyBitString(bitString);
				
				if (up2 >= 0 && up2 < 8){
					if (left >= 0 && left < 8)
						bitString[up2][left] = '1';
					if (right >= 0 && right < 8)
						bitString[up2][right] = '1';
					
					bitString[up2][f] = '1';
				}
				BitBoardGen::BITBOARD_KING_AHEAD[side][r * 8 + f][1] = bitboardFromBitString(bitString);
			}
		}
	}
}

U64 BitBoardGen::BITBOARD_KING_REGION[2][64];
void BitBoardGen::generateKingRegion(){
	char bitString[8][8];
	int king_ahead[] = {2, -2};

	for (int side = 0; side < 2; side++){
		for (int r = 0; r < 8; r++){
			for (int f = 0; f < 8; f++){
				emptyBitString(bitString);
				int left = f - 1;
				int right = f + 1;
				int up = r + 1;
				int down = r - 1;
				int up2 = r + king_ahead[side];

				//include king square
				bitString[r][f] = '1';
				
				if (up >= 0 && up < 8)
					bitString[up][f] = '1';
				if (down >= 0 && down < 8)
					bitString[down][f] = '1';
				if (left >= 0 && left < 8)
					bitString[r][left] = '1';
				if (right >= 0 && right < 8)
					bitString[r][right] = '1';
				if (up >= 0 && up < 8 && right >= 0 && right < 8)
					bitString[up][right] = '1';
				if (up >= 0 && up < 8 && left >= 0 && left < 8)
					bitString[up][left] = '1';
				if (down >= 0 && down < 8 && right >= 0 && right < 8)
					bitString[down][right] = '1';
				if (down >= 0 && down < 8 && left >= 0 && left < 8)
					bitString[down][left] = '1';
				//ahead
				if (up2 >= 0 && up2 < 8 && left >= 0 && left < 8)
					bitString[up2][left] = '1';
				if (up2 >= 0 && up2 < 8 && right >= 0 && right < 8)
					bitString[up2][right] = '1';
				if (up2 >= 0 && up2 < 8 && f >= 0 && f < 8)
					bitString[up2][f] = '1';
				
				BitBoardGen::BITBOARD_KING_REGION[side][r * 8 + f] = bitboardFromBitString(bitString);
			}
		}
	}
}

U64 BitBoardGen::BITBOARD_KING_ATTACKS[64];
void BitBoardGen::generateKing(){
	char bitString[8][8];
	
	for (int r = 0; r < 8; r++){
		for (int f = 0; f < 8; f++){
			emptyBitString(bitString);
			int left = f - 1;
			int right = f + 1;
			int up = r + 1;
			int down = r - 1;
			
			if (up >= 0 && up < 8)
				bitString[up][f] = '1';
			if (down >= 0 && down < 8)
				bitString[down][f] = '1';
			if (left >= 0 && left < 8)
				bitString[r][left] = '1';
			if (right >= 0 && right < 8)
				bitString[r][right] = '1';
			if (up >= 0 && up < 8 && right >= 0 && right < 8)
				bitString[up][right] = '1';
			if (up >= 0 && up < 8 && left >= 0 && left < 8)
				bitString[up][left] = '1';
			if (down >= 0 && down < 8 && right >= 0 && right < 8)
				bitString[down][right] = '1';
			if (down >= 0 && down < 8 && left >= 0 && left < 8)
				bitString[down][left] = '1';
			
			BitBoardGen::BITBOARD_KING_ATTACKS[r * 8 + f] = bitboardFromBitString(bitString);
		}
	}
}

U64 BitBoardGen::BITBOARD_PAWN_ATTACKS[2][64];
void BitBoardGen::generatePawns(){
	char bitString[8][8];
	
	//Pawn at Row 0 is used is MoveGen::isSquareAtacked
	for (int r = 0; r < 8; r++){
		for (int f = 0; f < 8; f++){
			emptyBitString(bitString);
			int left = f - 1;
			int right = f + 1;
			int up = r + 1;
			
			if (up >= 0 && up < 8 && left >= 0 && left < 8)
				bitString[up][left] = '1';
			if (up >= 0 && up < 8 && right >= 0 && right < 8)
				bitString[up][right] = '1';
			
			BitBoardGen::BITBOARD_PAWN_ATTACKS[Board::WHITE][r * 8 + f] = bitboardFromBitString(bitString);
		}
	}
	
	//Pawn at Row 7 is used is MoveGen::isSquareAtacked
	for (int r = 7; r >= 0; r--){
		for (int f = 0; f < 8; f++){
			emptyBitString(bitString);
			int left = f - 1;
			int right = f + 1;
			int up = r - 1;
			
			if (up >= 0 && up < 8 && left >= 0 && left < 8)
				bitString[up][left] = '1';
			if (up >= 0 && up < 8 && right >= 0 && right < 8)
				bitString[up][right] = '1';
			
			BitBoardGen::BITBOARD_PAWN_ATTACKS[Board::BLACK][r * 8 + f] = bitboardFromBitString(bitString);
		}
	}
}

U64 BitBoardGen::BITBOARD_DIRECTIONS[8][64];
void BitBoardGen::generateDirections(){
	char bitString[8][8];
	for (int k = 0; k < 8; k++){
		int *direction = BitBoardGen::DIRECTIONS[k];
			
		for (int r = 0; r < 8; r++){
			for (int f = 0; f < 8; f++){
				emptyBitString(bitString);
				int i = 1;
				int dy = direction[0];
				int dx = direction[1];
				
				while (r + i*dy >= 0 && r + i*dy < 8 && f + i*dx >= 0 && f + i*dx < 8){
					int rr = r + i*dy;
					int ff = f + i*dx;
					bitString[rr][ff] = '1';
					i++;
				}
				BitBoardGen::BITBOARD_DIRECTIONS[k][r * 8 + f] = bitboardFromBitString(bitString);
			}
		}
	}	
}

static void zeroBitString(char *bitString){
	for (int i = 0; i < 64; i++)
		bitString[i] = '0';
}

U64 BitBoardGen::RECT_LOOKUP[64][64];
void BitBoardGen::initRectLookUp(){
    char s[64];
	zeroBitString(s);
	
	for (int sq1 = 0; sq1 < 64; sq1 ++){
		for (int sq2 = 0; sq2 < 64; sq2++){
				
			int row1 = sq1/8;
			int row2 = sq2/8;
			int col1 = sq1 % 8;
			int col2 = sq2 % 8;
			int diff = sq1 >= sq2 ? (sq1 - sq2) : (sq2 - sq1);
			bool diag7 = (diff % 7) == 0 && diff/7 < 8; 
			bool diag9 = (diff % 9) == 0 && diff/9 < 8;
			
			int min = std::min(sq1, sq2);
			int max = std::max(sq1, sq2);
			int inc = -1;
			
			if (row1 == row2){
				inc = 1;
			}else if (col1 == col2){
				inc  = 8;
			}else if (diag7){
				inc = 7 ;
			}else if (diag9){
				inc  = 9;
			}
			
			if (inc > 0){
				zeroBitString(s);
				for (int m = min + inc; m < max; m+= inc)
					s[m] = '1';
				
				char bits[65];
				bits[64] = '\0';
				for (int i = 0; i < 64; i++)
					bits[i] = s[63 - i];
				
				RECT_LOOKUP[sq1][sq2] = fromBit(bits);
			}
		}
	}
}

U64 BitBoardGen::ADJACENT_FILES[8];
void BitBoardGen::generateAdjacentFiles(){
	char bitString[8][8];
	for (int c = 0; c < 8; c++){
		emptyBitString(bitString);
		
		for(int r = 0; r < 8; r++){
			if (c - 1 >= 0)
				bitString[r][c - 1] = '1';
			if (c + 1 < 8)
				bitString[r][c + 1] = '1';
		}
		BitBoardGen::ADJACENT_FILES[c] = bitboardFromBitString(bitString);
	}
}

U64 BitBoardGen::FRONT_SPAN[2][64];
void BitBoardGen::generateFrontSpan(){
	char bitString[8][8];
	int dy[] = {1, -1};
	
	for (int side = 0; side < 2; side++){
		
		for (int sq = 0; sq < 64; sq++){
			emptyBitString(bitString);

			int rr = sq/8;
			int c = sq % 8;
			
			for(int r = rr + dy[side]; r >= 0 && r < 8; r+=dy[side]){
				bitString[r][c] = '1';
				if (c - 1 >= 0)
					bitString[r][c - 1] = '1';
				if (c + 1 < 8)
					bitString[r][c + 1] = '1';
			}			
			BitBoardGen::FRONT_SPAN[side][sq] = bitboardFromBitString(bitString);
		}
	}
}

U64 BitBoardGen::FRONT_ATTACK_SPAN[2][64];
void BitBoardGen::generateFrontAttackSpan(){
	char bitString[8][8];
	int dy[] = {1, -1};
	
	for (int side = 0; side < 2; side++){
		
		for (int sq = 0; sq < 64; sq++){
			emptyBitString(bitString);

			int rr = sq/8;
			int c = sq % 8;
			
			for(int r = rr + dy[side]; r >= 0 && r < 8; r+=dy[side]){
				//bitString[r][c] = '1'; for attack span the column is set to zero
				if (c - 1 >= 0)
					bitString[r][c - 1] = '1';
				if (c + 1 < 8)
					bitString[r][c + 1] = '1';
			}			
			BitBoardGen::FRONT_ATTACK_SPAN[side][sq] = bitboardFromBitString(bitString);
		}
	}
}

U64 BitBoardGen::SQUARES_AHEAD[2][64];
U64 BitBoardGen::SQUARES_BEHIND[2][64];
void BitBoardGen::generateAheadBehind(){

	for (int side = 0; side < 2; side++){
		for (int i = 0; i < 64; i++){			
			SQUARES_AHEAD[side][i] = FRONT_SPAN[side][i] & (~FRONT_ATTACK_SPAN[side][i]);		
		}
	}
	for (int side = 0; side < 2; side++){
		for (int i = 0; i < 64; i++){			
			SQUARES_BEHIND[side][i] = SQUARES_AHEAD[side^1][i];
		}
	}
}

U64 BitBoardGen::SQUARES[64];
void BitBoardGen::initSquares(){
	for (int i = 0; i < 64; i++)
		SQUARES[i] = ONE << i;
}

int BitBoardGen::DISTANCE_SQS[64][64];
int BitBoardGen::DISTANCE_MAN[64][64];
void BitBoardGen::initDistances(){
	for (int i = 0; i < 64; i++){
		for (int j = 0; j < 64; j++){
			int ri = i >> 3;
			int fi = i & 7;
			int rj = j >> 3;
			int fj = j & 7;

			int rd = abs(rj - ri);
			int fd = abs(fj - fi);
			DISTANCE_MAN[i][j] = rd + fd;
			DISTANCE_SQS[i][j] = std::max(rd, fd);
		}
	}
}	

U64 BitBoardGen::LINES_BB[64][64];
void BitBoardGen::initLines(){
	for (int square1 = 0; square1 < 64; square1++){
		for (int square2 = 0; square2 < 64; square2++){
			
			int f1 = square1 & 7;
			int f2 = square2 & 7;
			int r1 = square1 >> 3;
			int r2 = square2 >> 3;
			
			int rMin = std::min(r1, r2);
			int rMax = std::max(r1, r2);
			int fMin = std::min(f1, f2);
			int fMax = std::max(f1, f2);
			
			bool AreOnsameFile = f1 == f2;
			bool AreOnsameRank = r1 == r2;
			bool AreOnSameDiag = fMax - fMin == rMax - rMin;
			
			U64  l = 0;
			if(AreOnsameFile){
				for (int r = 0; r < 8; ++r)
					l |= SQUARES[r * 8 + f1];
			}
			else if(AreOnsameRank){
				for (int f = 0; f < 8; ++f)
					l |= SQUARES[r1 * 8 + f];
			}
			else if (AreOnSameDiag){
				bool positiveDiag = f2-f1 == r2-r1;
				int f = f1; 
				int r = r1;
				
				if (positiveDiag){
					while (f >= 0 && f < 8 && r >= 0 && r < 8){
						l |= SQUARES[r * 8 + f];
						++f;
						++r;
					}
					f = f1; r = r1;
					while (f >= 0 && f < 8 && r >= 0 && r < 8){
						l |= SQUARES[r * 8 + f];
						--f;
						--r;
					}
				}
				else{ //anti-diag
					while (f >= 0 && f < 8 && r >= 0 && r < 8){
						l |= SQUARES[r * 8 + f];
						++f;
						--r;
					}
					f = f1; r = r1;
					while (f >= 0 && f < 8 && r >= 0 && r < 8){
						l |= SQUARES[r * 8 + f];
						--f;
						++r;
					}
				}
			}
			LINES_BB[square1][square2] = l;
		}
	}
}

U64 BitBoardGen::PAWN_CONNECTED[2][64];
void BitBoardGen::initPawnConnected(){
	for (int i = 0; i < 64; i++){
		PAWN_CONNECTED[Board::WHITE][i] = 0;
		PAWN_CONNECTED[Board::BLACK][i] = 0;
	}
	
	for (int i = 8 ; i < 56; i++){
        int file = i % 8;
        
        if (file == 0){
            PAWN_CONNECTED[Board::WHITE][i] = (ONE << (i+1)) | (ONE << (i-7));
            PAWN_CONNECTED[Board::BLACK][i] = (ONE << (i+1)) | (ONE << (i+9));
        }
        
        else if (file == 7){
            PAWN_CONNECTED[Board::WHITE][i] = (ONE << (i-1)) | (ONE << (i-9));
            PAWN_CONNECTED[Board::BLACK][i] = (ONE << (i-1)) | (ONE << (i+7));
        }
        
        else {
            PAWN_CONNECTED[Board::WHITE][i] = (ONE << (i-1)) | (ONE << (i-9)) | (ONE << (i+1)) | (ONE << (i-7));
            PAWN_CONNECTED[Board::BLACK][i] = (ONE << (i-1)) | (ONE << (i+7)) | (ONE << (i+1)) | (ONE << (i+9));
        }
    }
}

U64 BitBoardGen::SPACE_MASK[2];
U64 BitBoardGen::QUEENSIDE_MASK;
U64 BitBoardGen::KINGSIDE_MASK;
void BitBoardGen::initSpaceMasks(){
	char bitString[8][8];

	//white rank 2 to 4
	emptyBitString(bitString);
	for (int r = 2; r <= 4; r++)
		for (int c = 0; c < 8; c++)
			bitString[r][c] = '1';

	SPACE_MASK[Board::WHITE] = bitboardFromBitString(bitString);	
	
	//black rank 3 to 5
	emptyBitString(bitString);
	for (int r = 3; r <= 5; r++)
		for (int c = 0; c < 8; c++)
			bitString[r][c] = '1';

	SPACE_MASK[Board::BLACK] = bitboardFromBitString(bitString);

	//Queenside
	emptyBitString(bitString);
	for (int col = 0; col < 4; col++)
		for (int r = 0; r < 8; r++)
			bitString[r][col] = '1';

	QUEENSIDE_MASK = bitboardFromBitString(bitString);

	//Kingside
	emptyBitString(bitString);
	for (int col = 4; col < 8; col++)
		for (int r = 0; r < 8; r++)
			bitString[r][col] = '1';

	KINGSIDE_MASK = bitboardFromBitString(bitString);
}

U64 BitBoardGen::LIGHT_DARK_SQS[2];
int BitBoardGen::COLOR_OF_SQ[64];
void BitBoardGen::initColorSquares(){
		char bitString[8][8];
		emptyBitString(bitString);
		
		for (int i = 0; i < 8; i++){
				for (int  j = 0; j < 8; j++){
						if ((i + j) % 2){
							bitString[i][j] = '1';
							COLOR_OF_SQ[i * 8 + j] = 0;
						} else{
							COLOR_OF_SQ[i * 8 + j] = 1;
						}
				}
		}
		LIGHT_DARK_SQS[0] = bitboardFromBitString(bitString);
		LIGHT_DARK_SQS[1] = ~LIGHT_DARK_SQS[0];
}

/*
U64 BitBoardGen::ROOK_RAYS[64];
U64 BitBoardGen::BISHOP_RAYS[64];
U64 BitBoardGen::QUEEN_RAYS[64];
void BitBoardGen::initSliderRays(){

	for(int i = 0; i < 64; i++){
		ROOK_RAYS[i] = BITBOARD_DIRECTIONS[IDX_UP][i] | BITBOARD_DIRECTIONS[IDX_DOWN][i] 
		| BITBOARD_DIRECTIONS[IDX_RIGHT][i] | BITBOARD_DIRECTIONS[IDX_LEFT][i];

		BISHOP_RAYS[i] = BITBOARD_DIRECTIONS[IDX_UP_LEFT][i] | BITBOARD_DIRECTIONS[IDX_UP_RIGHT][i] 
		| BITBOARD_DIRECTIONS[IDX_DOWN_LEFT][i] | BITBOARD_DIRECTIONS[IDX_DOWN_RIGHT][i];

		QUEEN_RAYS[i] = ROOK_RAYS[i] | BISHOP_RAYS[i];
	}
}
*/

void BitBoardGen::initAll(){
	BitBoardGen::generateRanks();
	BitBoardGen::generateFiles();
	BitBoardGen::generateKnight();
	BitBoardGen::generateKingRegion();
	BitBoardGen::generateKing();
	BitBoardGen::generatePawns();
	BitBoardGen::generateCastleMask();
	BitBoardGen::generateDirections();
	BitBoardGen::generateWrapFiles();
	BitBoardGen::initRectLookUp();
	BitBoardGen::initSquares();
	BitBoardGen::generateFrontSpan();
	BitBoardGen::generateFrontAttackSpan();
	BitBoardGen::generateAheadBehind();
	BitBoardGen::generateAdjacentFiles();
	BitBoardGen::initDistances();
	BitBoardGen::generateKingAhead();
	BitBoardGen::initLines();
	BitBoardGen::initPawnConnected();
	BitBoardGen::initSpaceMasks();
	BitBoardGen::initColorSquares();
	//BitBoardGen::initSliderRays();
}