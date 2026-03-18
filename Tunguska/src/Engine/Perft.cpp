#include "Engine/Perft.h"
#include "Engine/FenParser.h"
#include "Engine/MoveGen.h"
#include "Engine/Zobrist.h"
#include "Engine/BitBoardGen.h"
#include <iostream>
#include <fstream>
#include <assert.h>
#include "Engine/StringUtils.h"
#include "Engine/Evaluation.h"

U64 Perft::TOTAL_NODES = 0;

U64 Perft::perft_pseudoTest(Board *board, int depth, int height){

	if (!depth)
		return 1;

	int side = board->state.currentPlayer;
	int ks = board->kingSQ[side];
	bool atCheck = MoveGen::isSquareAttacked(board, ks, side^1);

	MoveList moves;	
	MoveGen::pseudoLegalMoves(board, side, moves, atCheck);
	U64 result = 0;

    for (int i = 0; i < moves.size(); i++){
    	BoardState undo = board->makeMove(moves.get(i));
    	U64 count = 0;

    	if (!undo.valid)
    		continue;
    	
    	count = perft_pseudoTest(board, depth - 1, height + 1);

    	if(!height) {                
            for(int i = 0; i < height; ++i) {
                printf(" ");
            }                
           	std::cout << Move::toLongNotation(moves.get(i)) << " : " << count << std::endl;
        }
        result+= count;
        board->undoMove(moves.get(i), undo);
    }
    return result;
}

bool Perft::perft_pseudo(std::string fen, int depth, U64 expected){
	Board board = FenParser::parseFEN(fen);
	U64 nodes = 0;

	for(int i = 1; i <= depth; ++i) {
        clock_t start = clock();

        nodes = perft_pseudoTest(&board, i, 0);
        clock_t end = clock();

        if(!(end - start)) {
            end = start + 1;
        }
        double elapsed_secs = double(end - start)/CLOCKS_PER_SEC;        
        std::cout << "Perft " << i << ": " << nodes << " Time: " << elapsed_secs <<  " secs." << std::endl;
    }
    std::string status = (nodes == expected) ? " Success": " Fail";
    std::cout <<  "Result: " << nodes << " Expected: " << expected << status << std::endl;
    return nodes == expected;
}

Perft::Perft(std::string fen, int d, U64 n){
	FEN = fen;
	depth = d;
	nodes = n;
	board = FenParser::parseFEN(fen);
	verbose = true;
}

U64 Perft::perft(int depth_){

	int side = board.state.currentPlayer;
	int ks = board.kingSQ[side];
	bool atCheck = MoveGen::isSquareAttacked(&board, ks, side^1);

	MoveList moves;
	MoveGen::legalMoves(&board, side, moves, atCheck);
	int n_moves = moves.size();

	if (depth_ == 1)	
		return n_moves;

	U64 totalNodes = 0;
	for (int i = 0; i < moves.size(); i++){
		int wmat = board.material[0];
		int bmat = board.material[1];
		BoardState undo = board.makeMove(moves.get(i));

		totalNodes += perft(depth_ - 1);			
		board.undoMove(moves.get(i), undo);

		assert(wmat == board.material[0] && bmat==board.material[1]);
	}
	return totalNodes;
}

void Perft::divide(std::string fen, int depth){
                
  if (depth <= 0){
      std::cout << "Depth should be > 0." << std::endl;
      exit(0);
  }
  
  Board board = FenParser::parseFEN(fen);
  int side = board.state.currentPlayer;
  int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);
  bool atCheck = MoveGen::isSquareAttacked(&board, ks, side^1);

  MoveList moves;
  MoveGen::legalMoves(&board, side, moves, atCheck);
  int nodes = 0;
  depth-= 1;
                
  if (depth == 0){
       for (int i = 0; i < moves.size(); i++){
          std::cout << Move::toLongNotation(moves.get(i)) << ": 1" << std::endl;
          nodes++;
       }
  }
  else{
       for (int i = 0; i < moves.size(); i++){
           BoardState undo = board.makeMove(moves.get(i));
           Perft p(board.toFEN(), depth, 0);
           p.verbose = false;
           p.run();
           board.undoMove(moves.get(i), undo);
           std::cout << Move::toLongNotation(moves.get(i)) << ": " << p.result << std::endl;
           nodes+= p.result;
       }
  }
    std::cout << "Total nodes: " << nodes << std::endl;
}

bool Perft::run(){
	result = perft(depth);
	TOTAL_NODES += result;
	ok = result == nodes;
	std::string status = ok ? " Success": " Fail";

	if (verbose)
		std::cout <<  "Result: " << result << " Expected: " << nodes << status << std::endl;
	return ok;
}

void Perft::runAll(std::string test_file){
	std::ifstream file(test_file);
    std::string line;
    clock_t begin = clock();
	
    while (std::getline(file, line)){
        std::vector<std::string> tokens = splitString(line, ";");
		std::string fen = tokens.at(0);
		
		for (int i = 1; i < tokens.size(); i++){
			std::vector<std::string> DN = splitString(trim(tokens[i]), " ");
			int depth = std::stoi(DN.at(0));
			U64 nodes = std::stoull(DN.at(1));

			bool ok = Perft(fen, depth, nodes).run();
			//bool ok = perft_pseudo(fen, depth, nodes);
									
			if(!ok){
				std::cout << "Perft fail at FEN: " << fen << std::endl;
				return;
			}			
		}		
    }
    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    std::cout << "Total time: " << elapsed_secs << " Total nodes: " << Perft::TOTAL_NODES << std::endl;
	printf("Perft ok.\n");
}

