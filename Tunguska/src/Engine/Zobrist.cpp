#include "Engine/Zobrist.h"

std::random_device Zobrist::rd;
std::mt19937_64 Zobrist::gen;
std::uniform_int_distribution<U64> Zobrist::dis;
U64 Zobrist::pieceKeys[64][12];
U64 Zobrist::castleKeys[16];
U64 Zobrist::epKeys[8];
U64 Zobrist::sideBlackKey;

U64 Zobrist::getKey(const Board& board){
	U64 key = 0;

	//pieces
	for (int sq = 0; sq < 64; sq++){
		int piece = board.board[sq];
		if (piece != Board::EMPTY)
			key^= pieceKeys[sq][piece - 2];
	}

	//castle
	key^= castleKeys[board.state.castleKey];

	//ep
	int epSq = board.state.epSquare;
	if (epSq != 0)
		key^= epKeys[epSq % 8];

	//side 
	if (board.state.currentPlayer == Board::BLACK)
		key^= sideBlackKey;

	return key;
}

U64 Zobrist::random64(){
	return dis(gen);
}

void Zobrist::init_keys(){
	//piece keys
	for (int sq = 0; sq < 64; sq++){
		for (int p = 0; p < 12; p++){
			pieceKeys[sq][p] = random64();
		}
	}

	//castle keys
	for (int i = 0; i < 16; i++)
		castleKeys[i] = random64();

	//ep
	for (int i = 0; i < 8; i++)
		epKeys[i] = random64();

	//side key
	sideBlackKey = random64();
}

U64 Zobrist::xorFromTo(U64 key, int piece, int from, int to){
	key ^= pieceKeys[from][piece - 2];
	key ^= pieceKeys[to][piece - 2];
	return key;
}

U64 Zobrist::xorSquare(U64 key, int piece, int sq){
	key ^= pieceKeys[sq][piece - 2];
	return key;
}

U64 Zobrist::xorEP(U64 key, int sq){
	key ^= epKeys[sq % 8];
	return key;
}

U64 Zobrist::xorCastle(U64 key, int castle){
	key^= castleKeys[castle];
	return key;
}

U64 Zobrist::xorSide(U64 key){
	return key ^ sideBlackKey;
}
