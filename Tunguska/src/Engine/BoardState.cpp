#include "Engine/BoardState.h"

int BoardState::CASTLE_KS[2] = {WK_CASTLE, BK_CASTLE};
int BoardState::CASTLE_QS[2] = {WQ_CASTLE, BQ_CASTLE};

bool BoardState::can_castle_ks(int side){
	return castleKey & CASTLE_KS[side];
}

bool BoardState::can_castle_qs(int side){
	return castleKey & CASTLE_QS[side];
}

bool BoardState::white_can_castle_ks(){
	return castleKey & WK_CASTLE;
}

bool BoardState::white_can_castle_qs(){
	return castleKey & WQ_CASTLE;
}

bool BoardState::black_can_castle_ks(){
	return castleKey & BK_CASTLE;
}

bool BoardState::black_can_castle_qs(){
	return castleKey & BQ_CASTLE;
}
/*
void BoardState::set_white_castle_ks(){
	castleKey|= WK_CASTLE;
}

void BoardState::set_white_castle_qs(){
	castleKey|= WQ_CASTLE;
}

void BoardState::set_black_castle_ks(){
	castleKey|= BK_CASTLE;
}

void BoardState::set_black_castle_qs(){
	castleKey|= BQ_CASTLE;
}
*/