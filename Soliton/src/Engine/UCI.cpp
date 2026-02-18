#include "Engine/UCI.h"
#include "Engine/Search.h"
#include "Engine/Evaluation.h"
#include "Engine/TestSuite.h"
#include "Engine/EvalFen.h"
#include "Engine/Perft.h"
#include <iostream>
#include <sstream>

std::thread UCI::searchThread; 

void UCI::loop() {
    Evaluation::initAll();
    Board board = Board::fromStartPosition();
    HashTable* tt = new HashTable(256);
    board.setHashTable(tt);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name Soliton" << std::endl;
            std::cout << "id Fernando Mir" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        }
        else if (line == "ucinewgame") {
            // Ensure search is stopped before resetting
            if(searchThread.joinable()) {
                Search::stop();
                searchThread.join();
            }
            
            // 1. Clear the Transposition Table
            if (board.hashTable) {
                board.hashTable->reset();
            }

            // New board is constructed
            board = Board::fromStartPosition();
            board.setHashTable(tt);
        }
        else if (line.find("position") == 0) {
            parsePosition(line, board, tt);
        }
        else if (line.find("go") == 0) {
            parseGo(line, board);
        }
        else if (line == "stop") {
            if (searchThread.joinable()) {
                Search::stop();
                searchThread.join();
            }
        }
        else if (line.find("evaltest") == 0) {
            Evaluation::testEval("positions.fen");
        }
        else if (line.find("eval") == 0) {
            std::stringstream ss(line);
            std::string cmd, inputPath, outputPath;
            int depth;
            ss >> cmd; // This consumes the word "eval" from the stream
            std::string fl;

            // Now expects: eval <input> <output> <depth>
            if (ss >> inputPath >> outputPath >> depth) {         
                EvalFEN::eval(inputPath, outputPath, depth);
            }
            else {
                std::cout << "Error: Invalid format. Usage: eval <filename> <depth>" << std::endl;
            }
        }
        else if (line.find("perft") == 0) {
            Perft::runAll("perft.txt");
        }
        else if (line.find("bench") == 0) {
            TestSuite::runFile("bench.epd", 50);
        }
        else if (line == "quit") {
            // Ensure search is stopped before quitting
            if (searchThread.joinable()) {
                Search::stop();
                searchThread.join();
            }
            break;
        }
    }
}

void UCI::parsePosition(std::string line, Board& board, HashTable* tt) {
    // Remove "position " from the beginning
    std::string input = line.substr(9);

    // Find if a "moves" section exists
    size_t movesPos = input.find("moves ");

    std::string posStr;
    std::string movesStr = "";

    if (movesPos != std::string::npos) {
        // If moves exist, split the string into position and moves
        posStr = input.substr(0, movesPos);
        movesStr = input.substr(movesPos + 6);
    }
    else {
        // Otherwise, the whole string is the position
        posStr = input;
    }

    // Trim any trailing whitespace from the position string
    posStr.erase(posStr.find_last_not_of(" \n\r\t") + 1);

    // Now, parse the isolated position string
    if (posStr == "startpos") {
        board = Board::fromStartPosition();
    }
    else if (posStr.find("fen") == 0) {
        // The FEN is everything after "fen "
        std::string fen = posStr.substr(4);
        board = Board::fromFEN(fen);
    }

    // Re-link the hash table to the new board object
    board.setHashTable(tt);

    // Apply moves if they were found
    if (!movesStr.empty()) {
        board.applyMoves(movesStr);
    }
}

void UCI::parseGo(std::string line, Board& board) {
    int depth = Board::MAX_DEPTH; // Default to maximum depth
    long long movetime = -1;      // Default to no time limit

    std::stringstream ss(line);
    std::string token;

    while (ss >> token) {
        if (token == "depth") {
            ss >> depth;
        }
        else if (token == "movetime") {
            ss >> movetime;
        }
        /* TODO
        else if (token == "wtime") {
            // Basic time management placeholder
            long long wtime;
            ss >> wtime;
            if(board.state.currentPlayer == Board::WHITE) movetime = wtime / 30;
        }
        else if (token == "btime") {
             // Basic time management placeholder
             long long btime;
             ss >> btime;
             if(board.state.currentPlayer == Board::BLACK) movetime = btime / 30;
        }
        */
    }

    // Ensure any previous search is finished
    if (searchThread.joinable()) {
        searchThread.join();
    }

    // [board, depth, movetime] captures these variables by VALUE.
    // 'mutable' is required because iterativeDeepening modifies its local copy of the board.
    searchThread = std::thread([board, depth, movetime]() mutable {
        Search::iterativeDeepening(board, depth, movetime, true);
    });
}