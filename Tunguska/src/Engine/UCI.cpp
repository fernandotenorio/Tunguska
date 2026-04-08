#include "Engine/UCI.h"
#include "Engine/Search.h"
#include "Engine/Evaluation.h"
#include "Engine/Perft.h"
#include <iostream>
#include <sstream>

std::vector<std::thread> UCI::workers;
int num_threads = 1;

void UCI::loop() {
    Evaluation::initAll();
    Board board = Board::fromStartPosition();
    HashTable* tt = new HashTable();
    board.setHashTable(tt);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name Tunguska 2.1" << std::endl;
            std::cout << "id author Fernando Mir" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 512" << std::endl;
            std::cout << "option name Hash type spin default 256 min 1 max 8192" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        }
        else if (line.find("setoption") == 0) {
            parseSetOption(line, board, tt);
        }
        else if (line == "ucinewgame") {
            // Ensure search is stopped before resetting
            Search::stop();
            for (auto& t : workers) {
                if (t.joinable())
                    t.join();
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
            Search::stop();
            for (auto& t : workers) {
                if (t.joinable())
                    t.join();
            }
        }
        else if (line.find("evaltest") == 0) {
            Evaluation::testEval("positions.fen");
        }
        else if (line.find("perft") == 0) {
            Perft::runAll("perft.txt");
        }
        else if (line == "quit") {
            Search::stop();
            for (auto& t : workers) {
                if (t.joinable())
                    t.join();
            }
            delete tt; // delete the hash table before exiting!
            break;
        }
    }
}

void UCI::parseSetOption(std::string line, Board& board, HashTable*& tt) {
    std::stringstream ss(line);
    std::string token;
    
    ss >> token; // skip "setoption"
    ss >> token; // skip "name"

    std::string name = "";
    // Read the option name until we hit "value"
    while (ss >> token && token != "value") {
        if (!name.empty()) name += " ";
        name += token;
    }

    std::string valueStr = "";
    // Read the value parameter
    if (ss >> token) {
        valueStr = token; 
    }

    // Convert option name to lowercase for robust matching
    // (Some GUIs send "Threads", others send "threads")
    for (char& c : name) c = std::tolower(c);

    if (name == "threads") {
        try {
            int threads = std::stoi(valueStr);
            if (threads > 0) {
                num_threads = threads; // Update global thread count
            }
        } catch (const std::exception& e) {
            // Ignore invalid/non-integer values gracefully
        }
    }
    else if (name == "hash") {
        try {
            int hashSize = std::stoi(valueStr);
            if (hashSize > 0) {
                // Stop search and wait for threads to finish 
                // before deleting the memory they are actively reading/writing.
                Search::stop();
                for (auto& t : workers) {
                    if (t.joinable())
                        t.join();
                }
                workers.clear();

                // Free old table memory
                delete tt;
                
                // Allocate new table and attach it
                tt = new HashTable(hashSize);
                board.setHashTable(tt);
            }
        } catch (const std::exception& e) {}
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
    int depth = Board::MAX_DEPTH; 
    long long movetime = -1;      

    // time control parsing
    long long wtime = 0, btime = 0;
    long long winc = 0, binc = 0;
    int movestogo = 0;
    bool infinite = false;

    std::stringstream ss(line);
    std::string token;

    while (ss >> token) {
        if (token == "depth") ss >> depth;
        else if (token == "movetime") ss >> movetime;
        else if (token == "wtime") ss >> wtime;
        else if (token == "btime") ss >> btime;
        else if (token == "winc") ss >> winc;
        else if (token == "binc") ss >> binc;
        else if (token == "movestogo") ss >> movestogo;
        else if (token == "infinite") infinite = true;
    }

    if (wtime == 0 && btime == 0 && movetime == -1) {
        infinite = true;
    }

    // --- stop previous search ---
    Search::stop();
    for (auto& t : workers) {
        if (t.joinable())
            t.join();
    }

    workers.clear();

    // Reset global shared states before launching workers
    Search::stopped = false;
    Search::timeManager.init(wtime, btime, winc, binc, movestogo, board.state.currentPlayer == Board::WHITE, movetime, infinite);
    Search::timeManager.start();
    Search::totalNodes = 0;

    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([board, depth, i]() mutable {
            // Give the main thread (0) a tiny head start to populate TT
            if (i > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(i * 5));
            }
            Search searcher(i);
            bool isMain = (i == 0);
            searcher.iterativeDeepening(board, depth, isMain);
        });
    }
}