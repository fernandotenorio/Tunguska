#include "BoardState.h"
#include <stdio.h>

int BoardState::CASTLE_KS[2] = {WK_CASTLE, BK_CASTLE};
int BoardState::CASTLE_QS[2] = {WQ_CASTLE, BQ_CASTLE};

int BoardState::new_state(int epSquare, int halfMoves, int turn, int flags){
	return epSquare | (halfMoves << 6) | (turn << 13) | flags;
}

int BoardState::setEpSquare(int s, int ep){
	s&= ~0x3f;
	return s | ep ;
}

int BoardState::epSquare(int s){
	return s & 0x3f;
}

int BoardState::setHalfMoves(int s, int h){
	s&= ~(0x7f << 6);
	return s | (h << 6);
}

int BoardState::halfMoves(int s){
	return s >> 6 & 0x7f;
}

int BoardState::setCurrentPlayer(int s, int t){
	s&= ~(0x1 << 13);
	return s | t << 13;
}

int BoardState::currentPlayer(int s){
	return s >> 13 & 0x1;
}

bool BoardState::can_castle_ks(int s, int side){
	return (s & CASTLE_KS[side]) != 0;
}

bool BoardState::can_castle_qs(int s, int side){
	return (s & CASTLE_QS[side]) != 0;
}

bool BoardState::white_can_castle_ks(int s){
	return (s & WK_CASTLE) != 0;
}

bool BoardState::white_can_castle_qs(int s){
	return (s & WQ_CASTLE) != 0;
}

bool BoardState::black_can_castle_ks(int s){
	return (s & BK_CASTLE) != 0;
}

bool BoardState::black_can_castle_qs(int s){
	return (s & BQ_CASTLE) != 0;
}

int BoardState::castle_key(int s){
	return s >> 14;
}

void BoardState::print(int s){
	printf("ep square: %d\n", epSquare(s));
	printf("half moves:%d\n", halfMoves(s));
	printf("Turn: %d\n", currentPlayer(s));
	printf("WK: %d\n", white_can_castle_ks(s));
	printf("WQ: %d\n", white_can_castle_qs(s));
	printf("BK: %d\n", black_can_castle_ks(s));
	printf("BQ: %d\n", black_can_castle_qs(s));
}