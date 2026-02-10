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
#include "TestSuite.h"

const std::string UCI::ENGINE_NAME = "Tunguska 1.0";
const std::string UCI::ENGINE_AUTHOR = "Fernando Tenorio";

UCI::UCI(){}

UCI::~UCI(){}

void UCI::uciLoop(){
	std::string input;
	
	while (true){
		
		getline(std::cin, input);

		if ("uci" == input){
			inputUCI();
		} else if(startsWith(input, "setoption")){
			inputSetOption(input);
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
			pool.stopSearch();
			pool.waitForSearch();
			break;
		}
		else if ("stop" == input){
			pool.stopSearch();
			pool.waitForSearch();
		}
		else if ("perft" == input){
			Perft::runAll("perft.txt");
		}
		else if ("eval" == input){
			Evaluation::testEval("unique_sample.fen");
		}
		else if ("bench" == input){
			TestSuite::runFile("positions/STS1-STS15_LAN.EPD", 50);
		}
		else if (startsWith(input, "benchmt")){
			TestSuite::runFile("positions/STS1-STS15_LAN.EPD", 50, pool);
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
		time/= movesToGo;
		time-= 5;
		info.stopTime = (info.startTime + time + inc);
	}
	
	printf("time: %d start: %llu stop: %llu depth: %d timeset: %d\n",
			time, info.startTime, info.stopTime, info.depth, info.timeSet);

	pool.startSearch(board, info);
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
	std::cout << "option name Threads type spin default 1 min 1 max 64" << std::endl;
	std::cout << "option name Hash type spin default 256 min 1 max 4096" << std::endl;
	std::cout << "uciok" << std::endl;
}

void UCI::inputSetOption(std::string input){
	// Format: setoption name <name> value <value>
	if (stringContains(input, "Threads") || stringContains(input, "threads")){
		int val = parseField(input, "value");
		if (val > 0){
			pool.setThreadCount(val);
			std::cout << "info string Threads set to " << val << std::endl;
		}
	}
	else if (stringContains(input, "Hash") || stringContains(input, "hash")){
		int val = parseField(input, "value");
		if (val > 0){
			delete pool.sharedHash;
			pool.sharedHash = new HashTable(val);
		}
	}
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
	board.setHashTable(pool.sharedHash);

	if (stringContains(input, "moves")){
		input = input.substr(input.find("moves") + 6);
		board.applyMoves(input);
	}
	board.print();
	std::cout << board.toFEN() << std:: endl;
}

#include "MoveGen.h"

// interesting positions to test draw detection
// 8/8/2p1Q3/2P3pk/3P3p/6rP/PP3r1K/8 w - - 24 1
// 4R3/8/8/4k3/8/5q2/1K6/8 b - - 98 1
// position fen rn1qk2r/ppp1bppp/5n2/3p4/3P2b1/3B1N2/PPP2PPP/RNBQ1RK1 w kq - 6 7 moves h2h3 g4h5 b1d2 e8g8 d1e2 b8c6 e2e3 e7d6 d3f5 f8e8 e3b3 a8b8 g2g4 h5g6 f5g6 h7g6 c2c4 d6f4 c4c5 g6g5 b3d3 c6b4 d3b5 b4c2 a1b1 f6e4 b5a4 c2d4 a4d4 e4g3 d4d3 g3f1 g1f1 c7c6 d2b3 f4c1 b1c1 e8e4 b3d4 d8f6 f1g1 b8e8 b2b4 g8f8 a2a4 f6g6 c1d1 f8g8 g1g2 a7a6 b4b5 a6b5 a4b5 e8a8 d3d2 a8a4 d4f5 f7f6 f5d6 e4f4 d2e3 a4a2 b5b6 g8h7 d1d2 a2a8 d6b7 f4e4 e3d3 e4c4 d3g6 h7g6 f3d4 a8b8 d4c6 b8b7 c6a5 c4c5 a5b7 c5b5 b7d6 b5b6 d2d5 b6a6 d6f5 a6a7 g2f3 g6f7 f3e4 a7a4 d5d4 a4a7 f2f4 g5f4 e4f4 g7g5 f4e4 f7g6 d4d6 a7a4 f5d4 a4a1 d6d7 a1e1 e4f3 e1f1 f3e2 f1h1 d4f5 h1h2
//hard to find curious mate in 6
//b4bN1/4p1p1/1Q1p2K1/q1np4/3p1Rnk/6pr/4Rppr/3N4 w - -
int main(){
	BitBoardGen::initAll();
	Zobrist::init_keys();
	Evaluation::initAll();
	Search::initHeuristics();
	Magic::magicArraysInit();
	
	/*
	int mg=0, eg=0;
	AttackCache attCache;	
	Board board = Board::fromFEN("rnbqkb1r/ppp2ppp/4pn2/3p2B1/3PP3/2N5/PPP2PPP/R2QKBNR b KQkq -");
	board.print();
	Evaluation::kingAttack(board, mg, &attCache);
	Evaluation::threats(board, mg, eg, &attCache);
	*/

	/*
	//See test
	Board board = Board::fromFEN("8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -");
	board.print();
	int from = 14;
	int to = 42;
	int see = Search::see(&board, to, board.board[to], from, board.board[from]);
	printf("see = %d\n", see);
	*/

	//TestSuite::runFile("positions/STS1-STS15_LAN.EPD", 50);

	//Perft::perft_pseudo("r1b1k2r/pp1p1pp1/4p2p/1P5q/1Qn1NP1P/4BP2/P3K1P1/R4B1R b kq - 0 17", 2, 1507);

	/*
	Perft p = Perft(Board::START_POS, 6, 119060324, false);	
	clock_t begin = clock();
	p.run();
	clock_t end = clock();
	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	std::cout << "Perft(6) time (secs): " << elapsed_secs << std::endl;
	*/

	//Perft::divide("r1b1k2r/pp1p1pp1/4p2p/1P5q/1Qn1NP1P/4BP2/P3K1P1/R4B1R b kq - 0 17", 2);

	/*
	Board board = Board::fromFEN("1bq1n3/PP2Pk1P/2P5/8/1p1P4/3K2p1/pp2p2p/2R1B3 w - -");
	MoveList moves;
	MoveGen::pawnPromotions(&board, board.state.currentPlayer, moves, false);
	int legal = 0;

	for (int i=0; i < moves.size(); i++){				
		std::cout << "Trying Move: " << Move::toLongNotation(moves.get(i)) << std::endl;
		BoardState undo = board.makeMove(moves.get(i));

		if (!undo.valid){
			std::cout << "Move: " << Move::toLongNotation(moves.get(i)) << " is Illegal" << std::endl;
			continue;
		} else{
			std::cout << "Move: " << Move::toLongNotation(moves.get(i)) << " is Legal" << std::endl;
			board.undoMove(moves.get(i), undo);
			legal++;
		}
	}
	std::cout <<  "Legal moves: " << legal << std::endl;
	*/
	
	UCI uci;
	uci.uciLoop();
	return 0;
}