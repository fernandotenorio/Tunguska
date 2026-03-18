#ifndef PERFT_H
#define PERFT_H

#include "defs.h"
#include "Board.h"
#include <string>

class Perft{
	
	public:
		static void runAll(std::string test_file);
		std::string FEN;
		int depth;
		U64 nodes;
		U64 result;
		Board board;
		bool ok;
		bool verbose;
		bool eval;		
		static U64 TOTAL_NODES;

		static bool perft_pseudo(std::string fen, int depth, U64 expected);
		static U64 perft_pseudoTest(Board *board, int depth, int height);

		Perft(std::string fen, int depth, U64 nodes);
		U64 perft(int depth);
		bool run();
		static void divide(std::string fen, int depth);
};


#endif