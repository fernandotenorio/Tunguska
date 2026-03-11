#include "Engine/TestSuite.h"
#include "Engine/Evaluation.h"
#include "Engine/StringUtils.h"
#include "Engine/FenParser.h"
#include "Engine/HashTable.h"
#include "Engine/Search.h"
#include "Engine/Move.h"
#include <map>
#include <iostream>
#include <fstream>


void TestSuite::runFile(std::string fl, int movetime) {

	HashTable hashTable;
	std::ifstream file(fl);
	std::string line;

	std::map<std::string, int> total;
	std::map<std::string, int> correct;

	while (std::getline(file, line)) {
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
		testID.erase(testID.size() - 1);
		testID.erase(testID.size() - 1);
		testID.erase(testID.size() - 1);
		testID.erase(testID.size() - 1);
		testID.erase(testID.size() - 1);

		std::string pv = trim(tokens.at(tokens.size() - 2)); //-2 because each line ends with ;
		std::vector<std::string> tokens3 = splitString(pv, " ");
		std::string move = tokens3.at(1);

		//remove double quotes
		if (move.front() == '"')
			move.erase(0, 1);
		if (move.at(move.size() - 1) == '"')
			move.erase(move.size() - 1);

		Board board = FenParser::parseFEN(fen);
		board.setHashTable(&hashTable);
		hashTable.reset();

		Search searcher(0);
		int mv = searcher.iterativeDeepening(board, Board::MAX_DEPTH, movetime, true);
		std::string moveFound = Move::toLongNotation(mv);
		
		if (moveFound == move) {
			if (correct.count(testID) == 0) {
				correct[testID] = 1;
			}
			else {
				correct[testID] += 1;
			}
		}
		if (total.count(testID) == 0) {
			total[testID] = 1;
		}
		else {
			total[testID] += 1;
		}
	}

	//print results
	int tot = 0;
	int corr = 0;
	for (auto& kv : total) {
		std::cout << kv.first << " " << correct[kv.first] << "/" << kv.second << std::endl;
		tot += kv.second;
		corr += correct[kv.first];
	}
	printf("Found %d of %d (%.1f%%)\n", corr, tot, 100.0 * corr / tot);	
}
