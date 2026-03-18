#ifndef UCI_H
#define UCI_H

#include <string>
#include <vector>
#include <thread>
#include "Engine/Board.h"
#include "Engine/HashTable.h"

class UCI {
public:
    static void loop();
private:
    static void parseSetOption(std::string line, Board& board, HashTable*& tt);
    static void parsePosition(std::string line, Board& board, HashTable* tt);
    static void parseGo(std::string line, Board& board);
    static std::vector<std::thread> workers;
};

#endif