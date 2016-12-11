#include "FenParser.h"
#include "Board.h"
#include "Zobrist.h"
#include "StringUtils.h"
#include "BitBoardGen.h"
#include <iostream>

Board FenParser::parseFEN(std::string fen){
	
	fen = trim(fen);
	Board board;
	board.hashTable = NULL;
	std::vector<std::string> comps = splitString(fen, " ");
	std::vector<std::string> position = splitString(comps.at(0), "/");
	std::string player = comps.at(1);
	std::string castle = comps.at(2);
	std::string epSquare = comps.at(3);
	std::string halfMoves;
	std::string fullMoves;

	//Player
	if(player == "w")
		board.state = BoardState::setCurrentPlayer(board.state, Board::WHITE);
	else
		board.state = BoardState::setCurrentPlayer(board.state, Board::BLACK);
	
	//Castle
	if (castle == "-")
		board.state &= ~(BoardState::WK_CASTLE | BoardState::WQ_CASTLE | BoardState::BK_CASTLE | BoardState::BQ_CASTLE);
	if (stringContains(castle, "K"))
		board.state |= BoardState::WK_CASTLE;
	if (stringContains(castle, "Q"))
		board.state |= BoardState::WQ_CASTLE;
	if (stringContains(castle, "k"))
		board.state |= BoardState::BK_CASTLE;
	if (stringContains(castle, "q"))
		board.state |= BoardState::BQ_CASTLE;

	//Default half moves & full moves
	board.state = BoardState::setHalfMoves(board.state, 0);
	board.fullMoves = 1;

	//Half moves
	if (comps.size() > 4)
		board.state = BoardState::setHalfMoves(board.state, std::stoi(comps[4]));
		
	//Full Moves
	if (comps.size() > 5)
		board.fullMoves = std::stoi(comps[5]);

	//Ep
	if(epSquare == "-")
		board.state = BoardState::setEpSquare(board.state, 0);
	else
		board.state = BoardState::setEpSquare(board.state, Board::squareForCoord(epSquare));
		
	for (int i = 0; i < 8; i++){
		std::string row = position.at(i);

		if (row == "8"){
			for (int j = 0; j < 8; j++)
				board.board[(7 - i)*8 + j] = Board::EMPTY;
		}else{
			int col = 0;
			for (int j = 0; j < row.size(); j++){
				char c = row.at(j);
				int code =  Board::strToCode(c);
				
				if (code != Board::EMPTY){
					board.board[(7 - i)*8 + col++] = code;
				}
				else{
					for (int k = 0; k < parseIntChar(c); k++)
						board.board[(7 - i)*8 + col++] = Board::EMPTY;
				}
			}
		}
	}
		
	//setup bitboards
	for (int s = 0; s < 64; s++){
		if (board.board[s] == Board::EMPTY)
			continue;
		board.bitboards[board.board[s]] |= BitBoardGen::ONE << s;
		board.bitboards[board.board[s] & 1] |= BitBoardGen::ONE << s;
	}

	//zobrist key
	board.zKey = Zobrist::getKey(board);

	//hist ply
	board.histPly = 2*(board.fullMoves - 1) + (player == "w" ? 0:1);
	
	return board;
}