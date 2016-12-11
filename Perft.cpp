#include "Perft.h"
#include "FenParser.h"
#include "MoveGen.h"
#include "Zobrist.h"
#include "BitBoardGen.h"
#include <iostream>
#include <fstream>
#include <assert.h>
#include "StringUtils.h"

U64 Perft::TOTAL_NODES = 0;

Perft::Perft(std::string fen, int d, U64 n){
	FEN = fen;
	depth = d;
	nodes = n;
	board = FenParser::parseFEN(fen);
	verbose = true;
}

U64 Perft::perft(int depth_){

	int side = BoardState::currentPlayer(board.state);
	int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);
	bool atCheck = MoveGen::isSquareAttacked(board, ks, side^1);

	MoveList moves;
	MoveGen::legalMoves(board, side, moves, atCheck);
	int n_moves = moves.size();

	if (depth_ == 1)	
		return n_moves;

	U64 totalNodes = 0;
	for (int i = 0; i < moves.size(); i++){
		int undo = board.makeMove(moves.get(i));							
		totalNodes += perft(depth_ - 1);			
		board.undoMove(moves.get(i), undo);
	}
	return totalNodes;
}

void Perft::divide(std::string fen, int depth){
                
  if (depth <= 0){
      std::cout << "Depth should be > 0." << std::endl;
      exit(0);
  }
  
  Board board = FenParser::parseFEN(fen);
  int side = BoardState::currentPlayer(board.state);
  int ks = numberOfTrailingZeros(board.bitboards[Board::KING | side]);
  bool atCheck = MoveGen::isSquareAttacked(board, ks, side^1);

  MoveList moves;
  MoveGen::legalMoves(board, side, moves, atCheck);
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
           int undo = board.makeMove(moves.get(i));
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
	if (verbose)
		std::cout <<  "result: " << result << " ok: " << ok << " nodes: " << nodes << std::endl;
	return ok;
}

void Perft::runAll(std::string test_file){
	std::ifstream file(test_file);
    std::string line; 
	
    while (std::getline(file, line)){

        std::vector<std::string> tokens = splitString(line, ";");
		std::string fen = tokens.at(0);
		
		for (int i = 1; i < tokens.size(); i++){
			std::vector<std::string> DN = splitString(trim(tokens[i]), " ");
			int depth = std::stoi(DN.at(0));
			U64 nodes = std::stol(DN.at(1));

			bool ok = Perft(fen, depth, nodes).run();
			if(!ok){
				std::cout << "Perft fail at FEN: " << fen << std::endl;
				return;
			}
		}
    }
	printf("Perft ok.\n");
}

