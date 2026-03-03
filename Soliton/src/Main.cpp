#include <iostream>
#include "Engine/Perft.h"
#include "Engine/Evaluation.h"
#include "Engine/Magic.h"
#include "Engine/BitBoardGen.h"
#include "Engine/Zobrist.h"
#include "Engine/UCI.h"


int main() {
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Evaluation::initAll();
	Magic::magicArraysInit();
	Search::init_search();
	UCI::loop();
	
	// Board board = Board::fromFEN("8/4k3/3ppp2/8/8/1B6/B7/K7 w - - 0 1");
	// EvalInfo ei;
	// Evaluation::computeAttacks(board, ei);
	// std::cout << "Rook atacks " << ei.attackInfo.rookAttackersKing[0] << std::endl;
	// std::cout << "Kn atacks " << ei.attackInfo.knightAttackersKing[0] << std::endl;
	// std::cout << "Bishop atacks " << ei.attackInfo.bishopAttackersKing[0] << std::endl;
	// std::cout << "Queen atacks " << ei.attackInfo.queenAttackersKing[0] << std::endl;
	return 0;
}
