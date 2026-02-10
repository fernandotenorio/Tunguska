#ifndef UCI_H
#define UCI_H

#include "Board.h"
#include "HashTable.h"
#include "Search.h"
#include "ThreadPool.h"
#include <string>

class UCI{

	public:
		static const std::string ENGINE_NAME;
		static const std::string ENGINE_AUTHOR;
		UCI();
		~UCI();
		Board board;
		ThreadPool pool;
		SearchInfo info;

		void uciLoop();
		static int parseField(std::string input, std::string field);
		void inputGo(std::string input);
		void inputUCINewGame();
		void inputIsReady();
		void inputUCI();
		void inputPosition(std::string input);
		void inputSetOption(std::string input);

};


#endif