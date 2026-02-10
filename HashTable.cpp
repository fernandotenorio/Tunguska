#include "HashTable.h"
#include "Move.h"
#include <assert.h>
#include "Search.h"
#include "MoveGen.h"
#include <iostream>

void HashTable::initHash(int size){
	numEntries = (size * 0x100000)/sizeof(HashEntry);

	//if not power of two already
	if (numEntries & (numEntries - 1)) {

        numEntries--;
        for (int i = 1; i < 32; i = i*2)
            numEntries |= numEntries >> i;
        numEntries++;
        numEntries>>= 1;
    }
    numEntries_1 = numEntries - 1;
    table = new HashEntry[numEntries];

    for (U32 i = 0; i < numEntries; i++){
    	table[i] = HashEntry();
    }

    newWrite = 0;
    overWrite = 0;
    hit = 0;
    cut = 0;
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
	assert(index >= 0 && (U32)index <= board.hashTable->numEntries_1);

	HashEntry* entry = &board.hashTable->table[index];
	U64 d = entry->data;
	U64 k = entry->keyXorData;

	// XOR verification: recover key and check
	if ((k ^ d) == board.zKey) {
		return HashEntry::unpackMove(d);
	}
	return Move::NO_MOVE;
}

bool HashTable::probeHashEntry(Board& board, int *move, int *score, int alpha, int beta, int depth) {
	int index = (int)(board.zKey & board.hashTable->numEntries_1);

	HashEntry* entry = &board.hashTable->table[index];
	U64 d = entry->data;
	U64 k = entry->keyXorData;

	// XOR verification
	if ((k ^ d) == board.zKey) {
		*move = HashEntry::unpackMove(d);

		int entryDepth = HashEntry::unpackDepth(d);
		if (entryDepth >= depth){
			board.hashTable->hit++;

			*score = HashEntry::unpackScore(d);
			if(*score > ISMATE)
				*score -= board.ply;
            else if(*score < -ISMATE)
            	*score += board.ply;

            int entryFlags = HashEntry::unpackFlags(d);
            switch(entryFlags) {
                assert(*score >= -Search::INFINITE && *score <= Search::INFINITE);

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
                default:
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

	assert(index >= 0 && (U32)index <= board.hashTable->numEntries_1);
    assert(flags >= HFNONE && flags <= HFEXACT);
    assert(score >= -Search::INFINITE && score <= Search::INFINITE);
    assert(board.ply >=0 && board.ply < Board::MAX_DEPTH);

	HashEntry* entry = &board.hashTable->table[index];

	// Read existing entry to check depth-preferred replacement
	U64 oldData = entry->data;
	U64 oldKey = entry->keyXorData;
	U64 oldZKey = oldKey ^ oldData;

	if(oldData == 0 && oldKey == 0) {
		board.hashTable->newWrite++;
	} else {
		// Depth-preferred: keep deeper entries from different positions
		int oldDepth = HashEntry::unpackDepth(oldData);
		if (oldZKey != board.zKey && depth + 2 < oldDepth) {
			return;
		}
		board.hashTable->overWrite++;
	}

	if(score > ISMATE)
		score += board.ply;
    else if(score < -ISMATE)
    	score -= board.ply;

	U64 data = HashEntry::packData(move, score, depth, flags);
	entry->data = data;
	entry->keyXorData = board.zKey ^ data;
}

int HashTable::getPVLine(int depth, Board& board){
	int move = HashTable::probePvMove(board);
	int count = 0;
	BoardState undoList[Board::MAX_DEPTH];

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

	//undo
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
		table[i].keyXorData = 0;
	    table[i].data = 0;
	}
	newWrite = 0;
}
