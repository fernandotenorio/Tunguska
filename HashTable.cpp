#include "HashTable.h"
#include "Move.h"
#include <assert.h>
#include "Search.h"
#include "MoveGen.h"
#include <iostream>

BoardState HashTable::undoList[Board::MAX_DEPTH];


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

    for (int i = 0; i < numEntries; i++){
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
	assert(index >= 0 && index <= board.hashTable->numEntries_1);
	
	if( board.hashTable->table[index].zKey == board.zKey) {
		return board.hashTable->table[index].move;
	}
	return Move::NO_MOVE;
}

bool HashTable::probeHashEntry(Board& board, int *move, int *score, int alpha, int beta, int depth) {
	int index = (int)(board.zKey & board.hashTable->numEntries_1);

	if(board.hashTable->table[index].zKey == board.zKey) {
		*move = board.hashTable->table[index].move;

		if(board.hashTable->table[index].depth >= depth){
			board.hashTable->hit++;

			*score = board.hashTable->table[index].score;
			if(*score > ISMATE) 
				*score -= board.ply;
            else if(*score < -ISMATE) 
            	*score += board.ply;

            switch(board.hashTable->table[index].flags) {
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

	assert(index >= 0 && index <= board.hashTable->numEntries_1);
	//assert(depth >=1 && depth <= Board::MAX_DEPTH);
    assert(flags >= HFNONE && flags <= HFEXACT);
    assert(score >= -Search::INFINITE && score <= Search::INFINITE);
    assert(board.ply >=0 && board.ply < Board::MAX_DEPTH);
	
	HashEntry* entry = &board.hashTable->table[index];

	if(entry->zKey == 0) {
		board.hashTable->newWrite++;
	} else {
		// Depth-preferred: keep deeper entries from different positions
		if (entry->zKey != board.zKey && depth + 2 < entry->depth) {
			return;
		}
		board.hashTable->overWrite++;
	}

	if(score > ISMATE)
		score += board.ply;
    else if(score < -ISMATE)
    	score -= board.ply;

	entry->move = move;
    entry->zKey = board.zKey;
	entry->flags = flags;
	entry->score = score;
	entry->depth = depth;
}

int HashTable::getPVLine(int depth, Board& board){
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
		table[i].zKey = 0;
	    table[i].move = 0;
	    table[i].depth = 0;
	    table[i].score = 0;
	    table[i].flags = 0;
	}
	newWrite = 0;
}

