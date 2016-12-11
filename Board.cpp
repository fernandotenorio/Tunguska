#include "Board.h"
#include "FenParser.h"
#include "StringUtils.h"
#include "Zobrist.h"
#include "MoveGen.h"

#include <stdlib.h>
#include <iostream>

int Board::KS_CASTLE_ATTACK[2][2] = {{F1, G1}, {F8, G8}};
int Board::QS_CASTLE_ATTACK[2][2] = {{C1, D1}, {C8, D8}};
int Board::CASTLE_KS_SQ[2][4] = {{E1, G1, H1, F1}, {E8, G8, H8, F8}};
int Board::CASTLE_QS_SQ[2][4] = {{E1, C1, A1, D1}, {E8, C8, A8, D8}};
//int Board::CASTLE_SQS[2][2][4] = {CASTLE_KS_SQ, CASTLE_QS_SQ};
int Board::CASTLE_SQS[2][2][4] = {{{E1, G1, H1, F1}, {E8, G8, H8, F8}}, {{E1, C1, A1, D1}, {E8, C8, A8, D8}}};
std::string Board::RANKS[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
std::string Board::FILES[8] = {"a", "b", "c", "d", "e", "f", "g", "h"};
std::string Board::START_POS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Board::Board(){
	hashTable = NULL;

	for (int i = 0; i < 14; i++)
		bitboards[i] = 0;

	for (int i = 0; i < 64; i++)
		board[i] = 0;

	histPly = 0;
	ply = 0;
}

Board::~Board(){
	if (hashTable){
		//delete pvTable;
		//printf("PVTable deleted\n");
	}else{
		//printf("no PVTable to delete\n");
	}
}

Board Board::fromStartPosition(){
	return fromFEN(START_POS);
}

Board Board::fromFEN(std::string fen){
	return FenParser::parseFEN(fen);
}

/*
void Board::setPVTable(PVTable *tb){
	pvTable = tb;
}
*/

void Board::setHashTable(HashTable *tb){
	hashTable = tb;
}

int Board::makeNullMove(){
	int undo = state;
	
	//Save pos key
	zHist[histPly] = zKey;

	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	//erase ep square
	state = BoardState::setEpSquare(state, 0);

	int side = BoardState::currentPlayer(state);
	zKey = Zobrist::xorSide(zKey);
	state = BoardState::setCurrentPlayer(state, side ^ 1);

	histPly++;
	ply++;

	U64 tmpKey = Zobrist::getKey(*this);
	assert(tmpKey == zKey);

	return undo;
}

void Board::undoNullMove(int undo){
	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	state = undo;

	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	int side = BoardState::currentPlayer(state);
	int opp = side^1;

	zKey = Zobrist::xorSide(zKey);
	histPly--;
	ply--;

	U64 tmpKey = Zobrist::getKey(*this);
	assert(tmpKey == zKey);
}

int Board::makeMove(int move){

	int undo = state;
	
	//Save pos key
	zHist[histPly] = zKey;

	//Clear castle
	zKey = Zobrist::xorCastle(zKey, BoardState::castle_key(state));
	
	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	//erase ep square
	state = BoardState::setEpSquare(state, 0);
	
	int side = BoardState::currentPlayer(state);
	int opp = side^1;
	int from = Move::from(move);
	int to = Move::to(move);
	int capt = Move::captured(move);

	assert(capt != WHITE_KING);
	assert(capt != BLACK_KING);

	int movingPiece = board[from];	//not true if castle
	int promoteTo = Move::promoteTo(move);

	assert(promoteTo != WHITE_KING);
	assert(promoteTo != BLACK_KING);

	bool isEP = Move::isEP(move);
	bool isPJ = Move::isPJ(move);
	bool isCastle = Move::isCastle(move);
	
	if (isPJ){
			board[from] = EMPTY;
			board[to] = movingPiece;
			state = BoardState::setEpSquare(state, to + MoveGen::epCaptDiff[side]);
			bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], from);
			bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], to);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], to);

			//Zobrist xor out from xor in to, xor in new ep
			zKey = Zobrist::xorFromTo(zKey, movingPiece, from, to);
			zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));
		}
		else if(promoteTo != EMPTY){
			board[from] = EMPTY;
			board[to] = promoteTo;
			bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], from);
			assert(movingPiece == (PAWN | side));
			bitboards[promoteTo] = BitBoardGen::setBit(bitboards[promoteTo], to);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], to);

			//Zobrist xor out from, xor in promoted
			zKey = Zobrist::xorSquare(zKey, movingPiece, from);	
			zKey = Zobrist::xorSquare(zKey, promoteTo, to);
		}
		else if (isCastle){
			
			//sq = [k_from, k_to, rook_from, rook_to]
			int *sq = CASTLE_SQS[from][side];
			board[sq[0]] = EMPTY;
			board[sq[2]] = EMPTY;
			board[sq[1]] = KING | side;
			board[sq[3]] = ROOK | side;
			
			//@optimize
			if (side == WHITE)
				state&= ~(BoardState::WK_CASTLE | BoardState::WQ_CASTLE);
			else
				state&= ~(BoardState::BK_CASTLE | BoardState::BQ_CASTLE);
			
			bitboards[side | KING] = BitBoardGen::zeroBit(bitboards[side | KING], sq[0]);
			bitboards[side | ROOK] = BitBoardGen::zeroBit(bitboards[side | ROOK], sq[2]);
			bitboards[side | KING] = BitBoardGen::setBit(bitboards[side | KING], sq[1]);
			bitboards[side | ROOK] = BitBoardGen::setBit(bitboards[side | ROOK], sq[3]);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], sq[0]);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], sq[2]);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], sq[1]);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], sq[3]);

			//king
			zKey = Zobrist::xorFromTo(zKey, side | KING, sq[0], sq[1]);
			//rook
			zKey = Zobrist::xorFromTo(zKey, side | ROOK, sq[2], sq[3]);
		}
		else if(isEP){
			
			board[from] = EMPTY;
			board[to] = movingPiece;
			board[to + MoveGen::epCaptDiff[side]] = EMPTY;
			
			bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], from);
			bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], to);
			bitboards[opp | PAWN] = BitBoardGen::zeroBit(bitboards[opp | PAWN], to + MoveGen::epCaptDiff[side]);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], to);
			bitboards[opp] = BitBoardGen::zeroBit(bitboards[opp], to + MoveGen::epCaptDiff[side]);

			//Zobrist
			zKey = Zobrist::xorFromTo(zKey, movingPiece, from, to);
			zKey = Zobrist::xorSquare(zKey, opp | PAWN, to + MoveGen::epCaptDiff[side]);
		}
		else{
			board[from] = EMPTY;
			board[to] = movingPiece;
			bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], from);
			bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], to);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], to);

			//Zobrist			
			zKey = Zobrist::xorFromTo(zKey, movingPiece, from, to);	
		}
		
		//update capture BBs
		if (capt != EMPTY){
			bitboards[opp] = BitBoardGen::zeroBit(bitboards[opp], to);
			bitboards[capt] = BitBoardGen::zeroBit(bitboards[capt], to);

			//Zobrist			
			zKey = Zobrist::xorSquare(zKey, capt, to);			
		}
		
		//King moved
		if (movingPiece == WHITE_KING)
			state&= ~(BoardState::WK_CASTLE | BoardState::WQ_CASTLE);
		else if (movingPiece == BLACK_KING)
			state&= ~(BoardState::BK_CASTLE | BoardState::BQ_CASTLE);
		
		//Rook moved
		if (movingPiece == WHITE_ROOK && !isCastle){
			if(from == A1)
				state&= ~BoardState::WQ_CASTLE;
			else if (from == H1)
				state&= ~BoardState::WK_CASTLE;
		}
		else if(movingPiece == BLACK_ROOK && !isCastle){
			if(from == A8)
				state&= ~BoardState::BQ_CASTLE;
			else if (from == H8)
				state&= ~BoardState::BK_CASTLE;
		}
	
		//Rook captured
		if (capt == WHITE_ROOK){
			if (to == A1)
				state&= ~BoardState::WQ_CASTLE;
			else if (to == H1)
				state&= ~BoardState::WK_CASTLE;
		}
		else if (capt == BLACK_ROOK){
			if (to == A8)
				state&= ~BoardState::BQ_CASTLE;
			else if (to == H8)
				state&= ~BoardState::BK_CASTLE;
		}
		
		//half moves
		if (capt != EMPTY || movingPiece == WHITE_PAWN || movingPiece == BLACK_PAWN){
			if (!isCastle)
				state = BoardState::setHalfMoves(state, 0);
		} else{
			state = BoardState::setHalfMoves(state, 1 + BoardState::halfMoves(state));
		}

		zKey = Zobrist::xorSide(zKey);
		state = BoardState::setCurrentPlayer(state, opp);

		//set castle
		zKey = Zobrist::xorCastle(zKey, BoardState::castle_key(state));


		//DEBUG
		assert(Zobrist::getKey(*this) == zKey);
		
		histPly++;
		ply++;
		
		return undo;
}

void Board::undoMove(int move, int undo){
	//Zobrist clear castle
	zKey = Zobrist::xorCastle(zKey, BoardState::castle_key(state));

	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	state = undo;

	//Zobrist clear ep
	if (BoardState::epSquare(state) != 0)
		zKey = Zobrist::xorEP(zKey, BoardState::epSquare(state));

	//Zobrist clear castle
	zKey = Zobrist::xorCastle(zKey, BoardState::castle_key(state));
	
	int side = BoardState::currentPlayer(state);
	int opp = side^1;
	int from = Move::from(move);
	int to = Move::to(move);
	int capt = Move::captured(move);
	int movingPiece = board[to];	//true if not promotion
	int promoteTo = Move::promoteTo(move);
	bool isEP = Move::isEP(move);
	bool isPJ = Move::isPJ(move);
	bool isCastle = Move::isCastle(move);
	
	if (isPJ){
		board[from] = movingPiece;
		board[to] = EMPTY;
		bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], from);
		bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], to);
		bitboards[side] = BitBoardGen::setBit(bitboards[side], from);
		bitboards[side] = BitBoardGen::zeroBit(bitboards[side], to);

		//Zobrist xor out from xor in to, xor in new ep
		zKey = Zobrist::xorFromTo(zKey, movingPiece, to, from);
		//zKey = Zobrist.xorEP(zKey, BoardState.epSquare(this.state));
	}
	else if(promoteTo != EMPTY){
			board[from] = PAWN | side;
			board[to] = capt;
			bitboards[side | PAWN] = BitBoardGen::setBit(bitboards[side | PAWN], from);
			bitboards[promoteTo] = BitBoardGen::zeroBit(bitboards[promoteTo], to);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], to);

			//Zobrist xor out from, xor in promoted
			zKey = Zobrist::xorSquare(zKey, side | PAWN, from);
			zKey = Zobrist::xorSquare(zKey, promoteTo, to);
	}
	else if (isCastle){	
			//sq = [k_from, k_to, rook_from, rook_to]
			int *sq = CASTLE_SQS[from][side];
			board[sq[0]] = KING | side;
			board[sq[2]] = ROOK | side;
			board[sq[1]] = EMPTY;
			board[sq[3]] = EMPTY;
			
			bitboards[side | KING] = BitBoardGen::setBit(bitboards[side | KING], sq[0]);
			bitboards[side | ROOK] = BitBoardGen::setBit(bitboards[side | ROOK], sq[2]);
			bitboards[side | KING] = BitBoardGen::zeroBit(bitboards[side | KING], sq[1]);
			bitboards[side | ROOK] = BitBoardGen::zeroBit(bitboards[side | ROOK], sq[3]);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], sq[0]);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], sq[2]);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], sq[1]);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], sq[3]);

			//king
			zKey = Zobrist::xorFromTo(zKey, side | KING, sq[0], sq[1]);
			//rook
			zKey = Zobrist::xorFromTo(zKey, side | ROOK, sq[2], sq[3]);
	}
	else if(isEP){
			board[from] = movingPiece;
			board[to] = EMPTY;
			board[to + MoveGen::epCaptDiff[side]] = opp | PAWN;
			
			bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], from);
			bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], to);
			bitboards[opp | PAWN] = BitBoardGen::setBit(bitboards[opp | PAWN], to + MoveGen::epCaptDiff[side]);
			bitboards[side] = BitBoardGen::setBit(bitboards[side], from);
			bitboards[side] = BitBoardGen::zeroBit(bitboards[side], to);
			bitboards[opp] = BitBoardGen::setBit(bitboards[opp], to + MoveGen::epCaptDiff[side]);

			//Zobrist
			zKey = Zobrist::xorFromTo(zKey, movingPiece, to, from);
			zKey = Zobrist::xorSquare(zKey, opp | PAWN, to + MoveGen::epCaptDiff[side]);
	}
	else{
		board[from] = movingPiece;
		board[to] = capt;
		bitboards[movingPiece] = BitBoardGen::setBit(bitboards[movingPiece], from);
		bitboards[movingPiece] = BitBoardGen::zeroBit(bitboards[movingPiece], to);
		bitboards[side] = BitBoardGen::setBit(bitboards[side], from);
		bitboards[side] = BitBoardGen::zeroBit(bitboards[side], to);

		//Zobrist			
		zKey = Zobrist::xorFromTo(zKey, movingPiece, to, from);		
	}
	
	//update capture BBs
	if (capt != EMPTY){
		bitboards[opp] = BitBoardGen::setBit(bitboards[opp], to);
		bitboards[capt] = BitBoardGen::setBit(bitboards[capt], to);

		//Zobrist			
		zKey = Zobrist::xorSquare(zKey, capt, to);
	}
	zKey = Zobrist::xorSide(zKey);
	histPly--;
	ply--;

	//DEBUG
	U64 tmpKey = Zobrist::getKey(*this);
	assert(tmpKey == zKey);
}

bool Board::isRepetition(){
	//TODO test who is right
	//for(index = pos->hisPly - pos->fiftyMove; index < pos->hisPly-1; ++index) {
	for (int i = histPly - BoardState::halfMoves(state); i < histPly; i++){
			if (zHist[i] == zKey){
				return true;
			}
	}
	return false;
}

int Board::strToCode(char s){
	if (s == 'P')
		return WHITE_PAWN;
	else if (s == 'R')
		return WHITE_ROOK;
	else if (s == 'N')
		return WHITE_KNIGHT;
	else if (s == 'B')
		return WHITE_BISHOP;
	else if (s == 'Q')
		return WHITE_QUEEN;
	else if (s == 'K')
		return WHITE_KING;
	else if (s == 'p')
		return BLACK_PAWN;
	else if (s == 'r')
		return BLACK_ROOK;
	else if (s == 'n')
		return BLACK_KNIGHT;
	else if (s == 'b')
		return BLACK_BISHOP;
	else if (s == 'q')
		return BLACK_QUEEN;
	else if (s == 'k')
		return BLACK_KING;	
	else 
		return Board::EMPTY;
}

std::string Board::codeToStr(int code){
	if (code == WHITE_PAWN)
			return "P";
	else if (code == BLACK_PAWN)
		return "p"; 			
	else if (code == WHITE_ROOK)
		return "R";
	else if (code == BLACK_ROOK)
		return "r";
	else if (code == WHITE_KNIGHT)
		return "N";
	else if (code == BLACK_KNIGHT)
		return "n";
	else if (code == WHITE_BISHOP)
		return "B";
	else if (code == BLACK_BISHOP)
		return "b";
	else if (code == WHITE_QUEEN)
		return "Q";
	else if (code == BLACK_QUEEN)
		return "q";
	else if (code == WHITE_KING)
		return "K";
	else if (code == BLACK_KING)
		return "k";
	else if (code == EMPTY)
		return ".";
	else
		return "invalid code (codeToStr)";
}

int Board::squareForCoord(std::string coord){
	std::string rank = coord.substr(1, 2);
	std::string file = coord.substr(0, 1);
	int r = parseIntString(rank) - 1;
	int f = fileIndex(file);
	return r * 8 + f;
}

std::string Board::coordForSquare(int sq){
	int r = sq/8;
	int f = sq % 8;
	return FILES[f] + RANKS[r];
}

std::string Board::toFEN(){
    std::string fen = "";
    
    for (int i = 7; i >= 0; i--){
       int empty = 0;
       for (int j = 0; j < 8; j++){
                       int sq = i * 8 + j;
                       if (board[sq] == EMPTY){
                                       empty++;
                       }
                       else if (board[sq] != EMPTY && empty > 0){
                                       fen += "" + std::to_string(empty) + codeToStr(board[sq]);
                                       empty = 0;
                       }else{
                                       fen += codeToStr(board[sq]);
                       }
       }
       if(empty > 0)
            fen += std::to_string(empty);
       if (i > 0)
     		fen += "/";
    }
                
    if(BoardState::currentPlayer(state)== WHITE)
       fen+= " w ";
    else
		fen+= " b ";
    
    if (!(BoardState::white_can_castle_ks(state) || BoardState::white_can_castle_qs(state) || BoardState::black_can_castle_ks(state) || BoardState::black_can_castle_qs(state)))
                   fen += "-";
    if (BoardState::white_can_castle_ks(state))
                   fen += "K";
    if (BoardState::white_can_castle_qs(state))
                   fen += "Q";
    if (BoardState::black_can_castle_ks(state))
                   fen += "k";
    if (BoardState::black_can_castle_qs(state))
                   fen += "q";
                
    if (BoardState::epSquare(state) != 0)
                   fen += " " + coordForSquare(BoardState::epSquare(state));
    else
                   fen += " -";
    
    //todo increment at make move
    fullMoves = 1;
    fen += " " + std::to_string(BoardState::halfMoves(state)) + " " + std::to_string(fullMoves);
    return fen;
}

void Board::applyMoves(std::string movesString){
	
	std::vector<std::string> moves = splitString(trim(movesString), " ");
	int i = 0;

	for (auto& move:moves){

		int len = move.length();
		std::string from = "";
		std::string to = "";
		std::string promo = "";
		
		if (len == 4){
			from = move.substr(0, 2);
			to = move.substr(2);
		}else if (len == 5){
			from = move.substr(0, 2);
			to = move.substr(2, 2);
			promo = move.substr(4);
		}

		int who = BoardState::currentPlayer(state);
		int opp = who ^ 1;
		int sqFrom = Board::squareForCoord(from);
		int sqTo = Board::squareForCoord(to);

		if (promo != "")
			promo = who == WHITE ? toUpper(promo) : toLower(promo);

		const char *promo_cstr = promo.c_str();
		int promoCode = promo == "" ? 0 : Board::strToCode(promo_cstr[0]);
		int capt = board[sqTo];
		int mv = Move::get_move(sqFrom, sqTo, capt, promoCode, 0, 0, 0);

		//Castle: need castle flag check
		if ((move == "e1g1" || move == "e8g8") && (board[sqFrom] - who) == KING){
			mv = Move::get_move(0, who, 0, 0, 0, 0, Move::CASTLE_FLAG);
		}
		else if ((move == "e1c1" || move == "e8c8") && (board[sqFrom] - who) == KING){
			mv = Move::get_move(1, who, 0, 0, 0, 0, Move::CASTLE_FLAG);
		}
		//Quiet moves or captures
		else{
			int movingPiece = board[sqFrom] - who;
			int dy = sqFrom >= sqTo ? (sqFrom - sqTo) : (sqTo - sqFrom);
			int ep = BoardState::epSquare(state);

			//dead BUG was here: forgot movingPiece == PAWN, so pieces would trigger ep captures
			if (ep != 0 && sqTo == ep && movingPiece == PAWN)
				mv |= Move::EP_FLAG;				
			else if ((movingPiece == PAWN) && dy == 16)
				mv |= Move::PAWN_JUMP_FLAG;
		}
		makeMove(mv);
	}
	ply = 0;
}
		
void Board::print(){
	for (int r = 7; r >= 0; r--) {
			printf("%s  ", RANKS[r].c_str());
			for (int f = 0; f < 8; f++)
				std::cout << codeToStr(board[r*8 + f]) + " ";
			std::cout << std::endl;
	}
	printf("\n  ");
	for (int r = 0; r < 8; r++)	
		printf(" %s", FILES[r].c_str());
	std::cout << std::endl;
}
/*
int main(){
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Board board = FenParser::parseFEN(Board::START_POS);
	board.print();

	std::string moves = "d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 e6d5 c4d5 d7d6 g1f3 g7g6 c1g5 f8g7 f3d2 h7h6 g5h4 g6g5 h4g3 f6h5 d2c4 h5g3 h2g3 e8g8 e2e3 d8e7 f1e2 f8d8 e1g1 b8d7 a2a4 d7e5 c4e5 e7e5 a4a5 a8b8 a1a2 c8d7 c3b5 d7b5 e2b5 b7b6 a5a6 b8c8 d1d3 c8c7 b2b3 e5c3 d3c3 g7c3 a2c2 c3f6 g3g4 c7e7 c2c4 d8c8 g2g3 f6g7 f1d1 c8f8 d1d3 g8h7 g1g2 h7g6 d3d1 h6h5 g4h5 g6h5 g3g4 h5g6 c4c2 f8h8 b5d3 g6f6 g2g3 e7e8 d3b5 e8e4 c2c4 e4c4 b3c4 f6e7 b5a4 g7e5 g3f3 h8h4 d1g1 f7f5";

	board.applyMoves(moves);
	board.print();

	return 0;
}*/