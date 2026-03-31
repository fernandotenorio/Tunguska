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
	
	return 0;
}
