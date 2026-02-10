#ifndef TESTS_H
#define TESTS_H

#include <string>

class ThreadPool;

class TestSuite{
	public:
		static void runFile(std::string file, int movetime);
		static void runFile(std::string file, int movetime, ThreadPool& pool);
};

#endif