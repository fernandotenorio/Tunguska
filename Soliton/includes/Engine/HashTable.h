#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "Board.h"
#include "defs.h"
#include "Engine/Search.h"
#include <atomic>

#define ISMATE (Search::MATE - Board::MAX_DEPTH)

enum {HFNONE, HFALPHA, HFBETA, HFEXACT};

class HashEntry {
public:
    std::atomic<uint64_t> word1; // Stores: zKey ^ data
    std::atomic<uint64_t> word2; // Stores: packed data (move, score, depth, flags)

    HashEntry() {
        word1.store(0, std::memory_order_relaxed);
        word2.store(0, std::memory_order_relaxed);
    }
};

class HashTable {
public:
    HashTable();
    HashTable(int sizeMB);
    void initHash(int size);
    static bool probeHashEntry(Board& board, int *move, int *score, int alpha, int beta, int depth);
    static int probePvMove(Board& board);
    static void storeHashEntry(Board& board, const int move, int score, const int flags, const int depth);
    static int getPVLine(int depth, Board& board);
    static bool moveExists(Board& board, int move, int side);
    void reset();

    HashEntry *table;
    // Rounded down to a power of 2
    U32 numEntries;
    U32 numEntries_1;
    
    // Changing stats to lock-free atomics to let SMP update them safely
    std::atomic<int> newWrite;
    std::atomic<int> overWrite;
    std::atomic<int> hit;
    std::atomic<int> cut;
    const int DEFAULT_SIZE = 256;
};

#endif