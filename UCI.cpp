#include "UCI.h"
#include "StringUtils.h"
#include <iostream>
#include "defs.h"
#include <thread>
#include "Zobrist.h"
#include "Evaluation.h"
#include "BitBoardGen.h"
#include "Perft.h"
#include "Magic.h"

const std::string UCI::ENGINE_NAME = "Tunguska 1.0";
const std::string UCI::ENGINE_AUTHOR = "Fernando Tenorio";

UCI::UCI(){
	hashTable = new HashTable();
}

UCI::~UCI(){
	delete hashTable;
}

void UCI::uciLoop(){
	std::string input;
	
	while (true){
		
		getline(std::cin, input);

		if ("uci" == input){
			inputUCI();
		} else if(startsWith(input, "setoption")){
			//ignore
		} else if ("isready" == input){
			inputIsReady();
		} else if ("ucinewgame" == input){
			inputUCINewGame();
		} else if (startsWith(input, "position")){			
			inputPosition(input);
		}
		else if (startsWith(input, "go")){
			inputGo(input);
		}
		else if ("quit" == input){
			search.stop();
			break;
		}
		else if ("stop" == input){
			search.stop();
		}
		else if ("perft" == input){
			Perft::runAll("perft.txt");
		}
		else if ("eval" == input){
			Evaluation::testEval("unique_sample.fen");
		}
	}
}

int UCI::parseField(std::string input, std::string field){
	int val = -1;

	  if(stringContains(input, field)){
		std::vector<std::string> tokens = splitString(input, " ");
		for (int i = 0; i < tokens.size() - 1; i++){
			if (tokens.at(i) == field)
				return std::stoi(tokens[i + 1]);
		}
	}
	return val;
}

void UCI::inputGo(std::string input){
	int depth = -1;
	int movesToGo = 30;
	int moveTime = -1;	//fixed time per move
	int time = -1;
	int inc = 0;

	info.timeSet = false;
	
	if (stringContains(input, "infinite")){
	}
	if (stringContains(input, "binc") && board.state.currentPlayer == Board::BLACK){
		inc = parseField(input, "binc");
	}
	if (stringContains(input, "winc") && board.state.currentPlayer == Board::WHITE){
		inc = parseField(input, "winc");
	}
	if (stringContains(input, "wtime") && board.state.currentPlayer == Board::WHITE){
		time = parseField(input, "wtime");
	}
	if (stringContains(input, "btime") && board.state.currentPlayer == Board::BLACK){
		time = parseField(input, "btime");
	}
	if (stringContains(input, "movestogo")){
		movesToGo = parseField(input, "movestogo");
	}
	if (stringContains(input, "movetime")){
		moveTime = parseField(input, "movetime");
	}
	if (stringContains(input, "depth")){
		depth = parseField(input, "depth");
	}
	
	//fixed time per move set
	if(moveTime != -1) {
		time = moveTime;
		movesToGo = 1;
	}
	
	info.startTime = Search::getTime();
	info.depth = depth == -1 ? Board::MAX_DEPTH : depth;
	info.stopTime = 0;

	if(time != -1){
		info.timeSet = true;
		time /= movesToGo;
		time -= 10;
		info.stopTime = (info.startTime + time + inc);
	}
	
	printf("time: %d start: %llu stop: %llu depth: %d timeset: %d\n",
			time, info.startTime, info.stopTime, info.depth, info.timeSet);
			
	search.board = board;
	search.info = info;

	std::thread t([this]{
		search.search(true);
	});
	t.detach();
}

void UCI::inputUCINewGame(){
	inputPosition("position startpos");
}

void UCI::inputIsReady(){
	std::cout << "readyok" << std::endl;
}

void UCI::inputUCI(){
	std::cout << "id name " << ENGINE_NAME << std::endl;
	std::cout << "id author " << ENGINE_AUTHOR << std::endl;
	//options here
	std::cout << "uciok" << std::endl;
}

void UCI::inputPosition(std::string str){
	std::string input = str.substr(9) + " ";
	std::string FEN;
	
	if (stringContains(input, "startpos ")){
		input = input.substr(9);
		FEN = Board::START_POS;
	} else if(stringContains(input, "fen")){
		input = input.substr(4);
		FEN = input;
	}
	
	board = Board::fromFEN(FEN);
	board.setHashTable(hashTable);

	if (stringContains(input, "moves")){
		input = input.substr(input.find("moves") + 6);
		board.applyMoves(input);
	}
	board.print();
	std::cout << board.toFEN() << std:: endl;
}

#include "MoveGen.h"
#include "TestSuite.h"

// interesting positions
// 8/8/2p1Q3/2P3pk/3P3p/6rP/PP3r1K/8 w - - 24 1
// 4R3/8/8/4k3/8/5q2/1K6/8 b - - 98 1
// position fen rn1qk2r/ppp1bppp/5n2/3p4/3P2b1/3B1N2/PPP2PPP/RNBQ1RK1 w kq - 6 7 moves h2h3 g4h5 b1d2 e8g8 d1e2 b8c6 e2e3 e7d6 d3f5 f8e8 e3b3 a8b8 g2g4 h5g6 f5g6 h7g6 c2c4 d6f4 c4c5 g6g5 b3d3 c6b4 d3b5 b4c2 a1b1 f6e4 b5a4 c2d4 a4d4 e4g3 d4d3 g3f1 g1f1 c7c6 d2b3 f4c1 b1c1 e8e4 b3d4 d8f6 f1g1 b8e8 b2b4 g8f8 a2a4 f6g6 c1d1 f8g8 g1g2 a7a6 b4b5 a6b5 a4b5 e8a8 d3d2 a8a4 d4f5 f7f6 f5d6 e4f4 d2e3 a4a2 b5b6 g8h7 d1d2 a2a8 d6b7 f4e4 e3d3 e4c4 d3g6 h7g6 f3d4 a8b8 d4c6 b8b7 c6a5 c4c5 a5b7 c5b5 b7d6 b5b6 d2d5 b6a6 d6f5 a6a7 g2f3 g6f7 f3e4 a7a4 d5d4 a4a7 f2f4 g5f4 e4f4 g7g5 f4e4 f7g6 d4d6 a7a4 f5d4 a4a1 d6d7 a1e1 e4f3 e1f1 f3e2 f1h1 d4f5 h1h2
int main(){
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Evaluation::initAll();
	Search::initHeuristics();
	Magic::magicArraysInit();
	
	UCI uci;
	uci.uciLoop();
	return 0;
}