#include <iostream>
#include "Engine/Perft.h"
#include "Engine/Evaluation.h"
#include "Engine/Magic.h"
#include "Engine/BitBoardGen.h"
#include "Engine/Zobrist.h"
#include "Engine/UCI.h"
#include "Engine/MoveGen.h"


int main() {
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Evaluation::initAll();
	Magic::magicArraysInit();
	Search::init_search();
	UCI::loop();
	
	// Board board = Board::fromFEN("2K5/2P1k3/1R2n3/2r5/8/8/8/8 w - - 1 1");
	// MoveList moves;
	// MoveGen::pseudoLegalMoves(&board, board.state.currentPlayer, moves, false);

	// for (int i = 0; i < moves.size(); i++) {
    //     bool exist = HashTable::moveExists(board, moves.get(i), board.state.currentPlayer);
	// 	std::cout << Move::toLongNotation(moves.get(i)) << " " << (exist ? "ok": "invalid") << std::endl;
		
    // }
	
	return 0;
}
