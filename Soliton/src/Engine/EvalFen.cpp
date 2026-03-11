#include "Engine/EvalFen.h"
#include "Engine/Evaluation.h"
#include "Engine/StringUtils.h"
#include "Engine/FenParser.h"
#include "Engine/HashTable.h"
#include "Engine/Search.h"
#include "Engine/Move.h"
#include <map>
#include <iostream>
#include <fstream>


void EvalFEN::eval(std::string inputPath, std::string outputPath, int depth) {
    // 1. Open Input and Output streams
    std::ifstream inputFile(inputPath);
    std::ofstream outputFile(outputPath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open input file " << inputPath << std::endl;
        return;
    }
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not create output file " << outputPath << std::endl;
        return;
    }

    // 2. Setup Board and Hash
    HashTable hashTable;
    std::string line;

    // Optional: Write CSV Header
    // outputFile << "fen,score\n"; 

    while (std::getline(inputFile, line)) {
        // Trim and skip empty lines
        if (line.empty()) continue;

        // Parse FEN
        Board board = FenParser::parseFEN(line);
        board.setHashTable(&hashTable);

        // Run Search
        // Use a large timeout (e.g., 5000ms) to ensure depth is reached, 
        // or remove time check inside getScore entirely.
        Search searcher(0);
        int score = searcher.iterativeDeepeningScore(board, depth, 5000, true);

        // Filter invalid searches
        if (score == Search::INVALID_SCORE) {
            continue;
        }

        // Write directly to file
        // usage of "\n" is faster than std::endl because it doesn't force a flush
        outputFile << line << "," << score << "\n";
    }

    inputFile.close();
    outputFile.close();
    std::cout << "done evaluating files." << std::endl;
}
