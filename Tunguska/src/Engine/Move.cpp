#include "Engine/Move.h"
#include "Engine/Board.h"
#include <iostream>

int Move::get_move(int from, int to, int captured, int promoteTo, int isEP, int isPJ, int isCastle){
	return from | (to << 6) | (captured << 12) | (promoteTo << 17) | isEP | isPJ | isCastle;
}

int Move::from(int move){
	return move & 0x3F;
}

int Move::to(int move){
	return move >> 6 & 0x3F;
}

int Move::captured(int move){
	return move >> 12 & 0xF;
}

bool Move::isEP(int move){
	return (move & EP_FLAG) != 0;
}

bool Move::isPJ(int move){
	return (move & PAWN_JUMP_FLAG) != 0;
}

bool Move::isCastle(int move){
	return (move & CASTLE_FLAG) != 0;
}

int Move::promoteTo(int move){
	return move >> 17 & 0xF;
}

std::string Move::toNotation(int move){
	std::string mv = "";

	if (isCastle(move))
		mv = from(move) == 0 ? "O-O" : "O-O-O";
	else
		mv = Board::coordForSquare(from(move)) + Board::coordForSquare(to(move));
	return mv;
}

std::string Move::toLongNotation(int move){
	std::string mv = "";

	if (isCastle(move)){
		//Castle move convention
		if (to(move) == Board::WHITE)
			mv = from(move) == 0 ? "e1g1" : "e1c1";
		else
			mv = from(move) == 0 ? "e8g8" : "e8c8";
	}
	else{
		mv = Board::coordForSquare(from(move)) + Board::coordForSquare(to(move));
		if (promoteTo(move) != Board::EMPTY)
			mv += toLower(Board::codeToStr(promoteTo(move)));
	}
	return mv;
}

void Move::print(int move){
	std::cout << "from: " + Board::coordForSquare(from(move)) << "\n";
	std::cout << "to: " + Board::coordForSquare(to(move)) << "\n";
	printf("captured %d\n", Move::captured(move));
	printf("promote to: %d\n", Move::promoteTo(move));
	printf("isEP: %d\n", Move::isEP(move));
	printf("isCastle: %d\n", Move::isCastle(move));
	printf("isPJ: %d\n", Move::isPJ(move));
}