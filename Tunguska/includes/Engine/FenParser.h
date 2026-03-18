#ifndef FEN_PARSER_H
#define FEN_PARSER_H

class Board;

#include "Board.h"
#include <string>
#include <vector>

class FenParser{
	public:
		static Board parseFEN(std::string fen);
		
};

#endif