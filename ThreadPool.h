#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "Search.h"
#include "HashTable.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

class SearchThread {
public:
	Search search;
	int threadId;
	std::thread nativeThread;
	std::atomic<bool> searching;

	// Synchronization for parking threads
	std::mutex mtx;
	std::condition_variable cv;
	bool startFlag;
	bool exitFlag;

	SearchThread() : threadId(0), searching(false),
					 startFlag(false), exitFlag(false) {}
};

class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	void setThreadCount(int n);
	int getThreadCount() const { return numThreads; }

	// Start a search on all threads. Board and info describe the root position + time control.
	void startSearch(Board& rootBoard, SearchInfo& info, bool verbose = true);
	void stopSearch();
	void waitForSearch();

	// The shared hash table
	HashTable* sharedHash;

	// The shared stop signal
	std::atomic<bool> stopSignal;

	// Result: best move selected by voting across all threads
	int bestMove;

private:
	std::vector<SearchThread*> threads;
	int numThreads;
	bool verboseSearch = true;
	std::thread waiterThread;

	void threadLoop(SearchThread* st);
	void waiterFunc();
	void launchThreads();
	void destroyThreads();

	// Select best move by voting across all threads' results
	int selectBestMove();
};

#endif
