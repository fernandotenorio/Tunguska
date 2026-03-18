#include "Engine/FenParser.h"
#include "Engine/Board.h"
#include "Engine/Zobrist.h"
#include "Engine/StringUtils.h"
#include "Engine/BitBoardGen.h"
#include "Engine/Evaluation.h"
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
		board.state.currentPlayer = Board::WHITE;
	else
		board.state.currentPlayer = Board::BLACK;
	
	//Castle
	if (castle == "-")
		board.state.castleKey = 0;
	if (stringContains(castle, "K"))
		board.state.castleKey|= BoardState::WK_CASTLE;
	if (stringContains(castle, "Q"))
		board.state.castleKey|= BoardState::WQ_CASTLE;
	if (stringContains(castle, "k"))
		board.state.castleKey|= BoardState::BK_CASTLE;
	if (stringContains(castle, "q"))
		board.state.castleKey|= BoardState::BQ_CASTLE;

	//Default half moves & full moves
	board.state.halfMoves = 0;
	board.fullMoves = 1;

	//Half moves
	if (comps.size() > 4)
		board.state.halfMoves = std::stoi(comps[4]);
		
	//Full Moves
	if (comps.size() > 5)
		board.fullMoves = std::stoi(comps[5]);

	//Ep
	if(epSquare == "-")
		board.state.epSquare = 0;
	else
		board.state.epSquare = Board::squareForCoord(epSquare);
		
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

	//king squares
	int wk = numberOfTrailingZeros(board.bitboards[Board::WHITE_KING]);
	int bk = numberOfTrailingZeros(board.bitboards[Board::BLACK_KING]);
	board.kingSQ[0] = wk;
	board.kingSQ[1] = bk;

	//zobrist key
	board.zKey = Zobrist::getKey(board);
	board.state.zKey = board.zKey;

	//hist ply
	board.histPly = 0;//2*(board.fullMoves - 1) + (player == "w" ? 0:1);

	//count material
	for(int i = 0; i < 64; i++){
		if(! board.board[i])
			continue;
		if ((board.board[i] & 1) == Board::WHITE)
			board.material[0]+= Evaluation::PIECE_VALUES[board.board[i]];
		else
			board.material[1]+= abs(Evaluation::PIECE_VALUES[board.board[i]]);
	}
	return board;
}