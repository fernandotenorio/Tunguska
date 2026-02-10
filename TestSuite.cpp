#include "TestSuite.h"
#include "Evaluation.h"
#include "StringUtils.h"
#include <iostream>
#include <fstream>
#include "FenParser.h"
#include "HashTable.h"
#include "Search.h"
#include "Move.h"
#include "ThreadPool.h"
#include <map>

//TODO
std::string toAlgebraic(int move, Board& board){
	std::string mv = "";
	
	if (Move::isCastle(move))
		return (Move::from(move) == 0) ? "O-O" : "O-O-O";

	int from = Move::from(move);
	int to = Move::to(move);
	int captured = Move::captured(move);
	bool isEP = Move::isEP(move);
	int promoteTo = Move::promoteTo(move);

	//pawn moves
	if (board.board[from] == Board::WHITE_PAWN || board.board[from] == Board::BLACK_PAWN){
		if (promoteTo){
			if (captured){
				mv = Board::FILES[from] + "x" + Board::coordForSquare(to) + "=" + toUpper(Board::codeToStr(promoteTo));
			} else{
				mv = Board::coordForSquare(to) + "=" + toUpper(Board::codeToStr(promoteTo));
			}
		}
		else if (isEP || captured){
			mv = Board::FILES[from] + "x" + Board::coordForSquare(to);
		}
		else if(promoteTo){
			mv = Board::coordForSquare(to);
		}
	} 
	// piece move
	else{
		int piece = board.board[from];
		std::string pieceStr = toUpper(Board::codeToStr(piece));

		if (captured){

		}else{

		}
	}	

	return mv;
}


void TestSuite::runFile(std::string fl, int movetime){

	HashTable hashTable;
	std::ifstream file(fl);
    std::string line;

    std::map<std::string, int> total;
    std::map<std::string, int> correct;

    while (std::getline(file, line)){
    	std::vector<std::string> tokens = splitString(line, ";");
		std::string fen = tokens.at(0);

		std::vector<std::string> tokens2 = splitString(fen, "bm");
		fen = trim(tokens2[0]);

		std::string bm = trim(tokens2[1]);
		int idx = line.find("id ");	
		std::string testID = line.substr(idx);
		idx = testID.find(";");
		testID = trim(testID.substr(0, idx));

		//remove numbers
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);

		//if (!(stringContains(testID, "AKPC") || stringContains(testID, "pawns")))
		//	continue;

		std::string pv = trim(tokens.at(tokens.size() - 2)); //-2 because each line ends with ;
		std::vector<std::string> tokens3  = splitString(pv, " ");
		std::string move = tokens3.at(1);

		//remove double quotes
		if (move.front() == '"')
    		move.erase(0, 1);
    	if(move.at(move.size() - 1) == '"')
    		move.erase(move.size() - 1);		
		
		Board board = FenParser::parseFEN(fen);
		board.setHashTable(&hashTable);

		SearchInfo info;
		info.startTime = Search::getTime();
		info.stopTime = (info.startTime + movetime);
		info.depth = Board::MAX_DEPTH;
		info.timeSet = true;
		Search search(board, info);
		int mv = search.search(false);		
		std::string moveFound = Move::toLongNotation(mv);		
		//hashTable.reset();

		//std::cout << "Found: " << moveFound << " Best: " << move << std::endl;

		if (moveFound == move){
			if (correct.count(testID) == 0)
				correct[testID] = 1;
			else
				correct[testID]+= 1;
		}
		if (total.count(testID) == 0)
			total[testID] = 1;
		else
			total[testID]+= 1;

    }

    //print results
    int tot = 0;
    int corr = 0;
    for(auto& kv : total){
    	std::cout << kv.first << " " << correct[kv.first] << "/" << kv.second << std::endl;    	
    	tot+= kv.second;
    	corr+= correct[kv.first];
    }
    printf("Found %d of %d (%.1f%%)\n", corr, tot, 100.0*corr/tot);

}

void TestSuite::runFile(std::string fl, int movetime, ThreadPool& pool){

	std::ifstream file(fl);
    std::string line;

    std::map<std::string, int> total;
    std::map<std::string, int> correct;

    while (std::getline(file, line)){
    	std::vector<std::string> tokens = splitString(line, ";");
		std::string fen = tokens.at(0);

		std::vector<std::string> tokens2 = splitString(fen, "bm");
		fen = trim(tokens2[0]);

		std::string bm = trim(tokens2[1]);
		int idx = line.find("id ");
		std::string testID = line.substr(idx);
		idx = testID.find(";");
		testID = trim(testID.substr(0, idx));

		//remove numbers
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);
		testID.erase(testID.size()-1);

		std::string pv = trim(tokens.at(tokens.size() - 2));
		std::vector<std::string> tokens3  = splitString(pv, " ");
		std::string move = tokens3.at(1);

		//remove double quotes
		if (move.front() == '"')
    		move.erase(0, 1);
    	if(move.at(move.size() - 1) == '"')
    		move.erase(move.size() - 1);

		Board board = FenParser::parseFEN(fen);
		board.setHashTable(pool.sharedHash);

		// Clear hash between positions to avoid stale entries from previous position
		pool.sharedHash->reset();

		SearchInfo info;
		info.startTime = Search::getTime();
		info.stopTime = (info.startTime + movetime);
		info.depth = Board::MAX_DEPTH;
		info.timeSet = true;

		pool.startSearch(board, info, false);
		pool.waitForSearch();
		int mv = pool.bestMove;
		std::string moveFound = Move::toLongNotation(mv);

		if (moveFound == move){
			if (correct.count(testID) == 0)
				correct[testID] = 1;
			else
				correct[testID]+= 1;
		}
		if (total.count(testID) == 0)
			total[testID] = 1;
		else
			total[testID]+= 1;

    }

    //print results
    int tot = 0;
    int corr = 0;
    for(auto& kv : total){
    	std::cout << kv.first << " " << correct[kv.first] << "/" << kv.second << std::endl;
    	tot+= kv.second;
    	corr+= correct[kv.first];
    }
    printf("Found %d of %d (%.1f%%)\n", corr, tot, 100.0*corr/tot);
}