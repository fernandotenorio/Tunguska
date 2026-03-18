#include "Engine/HashTable.h"
#include "Engine/Move.h"
#include "Engine/MoveGen.h"
#include <assert.h>
#include <iostream>

void HashTable::initHash(int size){
    numEntries = (size * 0x100000)/sizeof(HashEntry);

    // If not a power of two already
    if (numEntries & (numEntries - 1)) {
        numEntries--;
        for (int i = 1; i < 32; i = i*2)
            numEntries |= numEntries >> i;
        numEntries++;
        numEntries>>= 1;
    }
    numEntries_1 = numEntries - 1;
    table = new HashEntry[numEntries];

    newWrite.store(0, std::memory_order_relaxed);
    overWrite.store(0, std::memory_order_relaxed);
    hit.store(0, std::memory_order_relaxed);
    cut.store(0, std::memory_order_relaxed);
    std::cout << "Hash Table size: " << numEntries * sizeof(HashEntry)/0x100000 << " MB" << std::endl;
}

HashTable::HashTable(int sizeMB){
    initHash(sizeMB);
}

HashTable::HashTable(){
    initHash(DEFAULT_SIZE);
}

int HashTable::probePvMove(Board& board){
    int index = (int)(board.zKey & board.hashTable->numEntries_1);
    assert(index >= 0 && index <= (int)board.hashTable->numEntries_1);
    
    HashEntry& entry = board.hashTable->table[index];

    // Lockless read: The acquire fence on word1 guarantees we don't accidentally pull a future word2
    uint64_t w1 = entry.word1.load(std::memory_order_acquire);
    uint64_t data = entry.word2.load(std::memory_order_relaxed);
    uint64_t key = w1 ^ data;
    
    if(key == board.zKey) {
        return (int)(uint32_t)(data & 0xFFFFFFFF); // Move occupies the first 32 bits
    }
    return Move::NO_MOVE;
}

bool HashTable::probeHashEntry(Board& board, int *move, int *score, int alpha, int beta, int depth) {
    int index = (int)(board.zKey & board.hashTable->numEntries_1);
    HashEntry& entry = board.hashTable->table[index];

    uint64_t w1 = entry.word1.load(std::memory_order_acquire);
    uint64_t data = entry.word2.load(std::memory_order_relaxed);
    uint64_t key = w1 ^ data;

    if(key == board.zKey) {
        // Unpack properties seamlessly
        int e_move  = (int)(uint32_t)(data & 0xFFFFFFFF);
        int e_score = (int)(int16_t)(uint16_t)((data >> 32) & 0xFFFF);
        int e_depth = (int)(int8_t)(uint8_t)((data >> 48) & 0xFF);
        int e_flags = (int)(int8_t)(uint8_t)((data >> 56) & 0xFF);

        *move = e_move;

        if(e_depth >= depth){
            board.hashTable->hit.fetch_add(1, std::memory_order_relaxed);

            *score = e_score;
            if(*score > ISMATE) 
                *score -= board.ply;
            else if(*score < -ISMATE) 
                *score += board.ply;

            switch(e_flags) {
                case HFALPHA: 
                    if(*score <= alpha) {
                        *score = alpha;
                        return true;
                    }
                    break;
                case HFBETA: 
                    if(*score >= beta) {
                        *score = beta;
                        return true;
                    }
                    break;
                case HFEXACT:
                    return true;
                    break;
                default: assert(false); 
                break;
            }
        }
    }
    return false;
}

void HashTable::storeHashEntry(Board& board, const int move, int score, const int flags, const int depth){
    if (depth >= Board::MAX_DEPTH)
        return;

    int index = (int)(board.zKey & board.hashTable->numEntries_1);
    HashEntry& entry = board.hashTable->table[index];

    // Atomically read the existing entry
    uint64_t w1_old = entry.word1.load(std::memory_order_relaxed);
    uint64_t data_old = entry.word2.load(std::memory_order_relaxed);
    uint64_t key_old = w1_old ^ data_old;

    if(key_old == board.zKey) {
        int old_depth = (int)(int8_t)(uint8_t)((data_old >> 48) & 0xFF);
        int old_flags = (int)(int8_t)(uint8_t)((data_old >> 56) & 0xFF);

        // SMP TRICK: Always Replace to keep data fresh, EXCEPT when a 
        // shallow bound tries to overwrite a deep EXACT (PV) score.
        if (depth + 2 < old_depth && old_flags == HFEXACT && flags != HFEXACT) {
            return; // Protect the high-quality PV node!
        }
    }

    if(key_old == 0) {
        board.hashTable->newWrite.fetch_add(1, std::memory_order_relaxed);
    } else {
        board.hashTable->overWrite.fetch_add(1, std::memory_order_relaxed);
    }
    
    if(score > ISMATE) score += board.ply;
    else if(score < -ISMATE) score -= board.ply;
    
    uint64_t data = 0;
    data |= (uint32_t)move;
    data |= ((uint64_t)(uint16_t)(int16_t)score) << 32;
    data |= ((uint64_t)(uint8_t)depth) << 48;
    data |= ((uint64_t)(uint8_t)flags) << 56;

    entry.word2.store(data, std::memory_order_relaxed);
    entry.word1.store(board.zKey ^ data, std::memory_order_release);
}

int HashTable::getPVLine(int depth, Board& board){
    BoardState undoList[Board::MAX_DEPTH];
    int move = HashTable::probePvMove(board);
    int count = 0;

    while (move != Move::NO_MOVE && count < depth){
        assert(count < Board::MAX_DEPTH);

        if (moveExists(board, move, board.state.currentPlayer)) {
            BoardState undo = board.makeMove(move);
            undoList[count] = undo;
            board.pvArray[count++] = move;              
        } else{
            break;
        }
        move = HashTable::probePvMove(board);
    }

    for (int i = count - 1; i >= 0; i--)
        board.undoMove(board.pvArray[i], undoList[i]);

    return count;
}

bool HashTable::moveExists(Board& board, int move, int side){   
    int ks = board.kingSQ[side];
    bool atCheck = MoveGen::isSquareAttacked(&board, ks, side^1);
    MoveList moves;
    MoveGen::pseudoLegalMoves(&board, side, moves, atCheck);
    U64 pinned = MoveGen::pinnedBB(&board, side, ks);

    for (int i = 0; i < moves.size(); i++){
        if (moves.get(i) == move && MoveGen::isLegalMove(&board, moves.get(i), side, atCheck, pinned)){
            return true;
        }
    }
    return false;
}

void HashTable::reset(){
    for (U64 i = 0; i < numEntries; i++){
        table[i].word1.store(0, std::memory_order_relaxed);
        table[i].word2.store(0, std::memory_order_relaxed);
    }
    newWrite.store(0, std::memory_order_relaxed);
}

HashTable::~HashTable() {
    delete[] table;
}