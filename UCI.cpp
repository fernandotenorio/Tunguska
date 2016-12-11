#include "UCI.h"
#include "StringUtils.h"
#include <iostream>
#include "defs.h"
#include <thread>
#include "Zobrist.h"
#include "Evaluation.h"
#include "BitBoardGen.h"
#include "Perft.h"

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
			std::exit(0);
		}
		else if ("stop" == input){
			search.stop();
		}
		else if ("perft" == input){
			Perft::runAll("perft.txt");
		}
		/* Mirror eval test
		else if ("eval" == input){
			Evaluation::testEval("positions.fen");
		}
		*/
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
	if (stringContains(input, "binc") && BoardState::currentPlayer(board.state) == Board::BLACK){
		inc = parseField(input, "binc");
	}
	if (stringContains(input, "winc") && BoardState::currentPlayer(board.state) == Board::WHITE){
		inc = parseField(input, "winc");
	}
	if (stringContains(input, "wtime") && BoardState::currentPlayer(board.state) == Board::WHITE){
		time = parseField(input, "wtime");
	}
	if (stringContains(input, "btime") && BoardState::currentPlayer(board.state) == Board::BLACK){
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
		time -= 50;
		info.stopTime = (info.startTime + time + inc);
	}
	
	printf("time: %d start: %llu stop: %llu depth: %d timeset: %d\n",
			time, info.startTime, info.stopTime, info.depth, info.timeSet);
			
	search.board = board;
	search.info = info;

	std::thread t([this]{
		search.search();
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
}

#include "MoveGen.h"

int main(){
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Search::initHeuristics();
	Evaluation::initAll();
	
	/*
	Perft p = Perft(Board::START_POS, 6, 119060324);
	clock_t begin = clock();
	p.run();
	clock_t end = clock();
	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	std::cout << "Perft(6) time: " << elapsed_secs << std::endl;
	*/
	
	UCI uci;
	uci.uciLoop();

	/*
	MoveList moves;
	Board board = FenParser::parseFEN("Q2k4/8/8/8/7K/8/7p/8 b - -");
	board.print();
	MoveGen::getEvasions(board, 1, moves);
	for (int i=0; i<moves.size(); i++){
		Move::print(moves.get(i));
		std::cout<<"------"<<std::endl;
	}
	*/

	//Perft::divide("1r3K2/2P5/5k2/8/8/8/8/8 w - -", 6);
	//Perft::divide("8/Pk6/8/8/8/8/6Kp/8 w - - 0 1", 6);
	
	return 0;
}