#include "ThreadPool.h"
#include "Move.h"
#include <unordered_map>
#include <iostream>

ThreadPool::ThreadPool() : bestMove(0), numThreads(1) {
	sharedHash = new HashTable();
	stopSignal.store(false);
	launchThreads();
}

ThreadPool::~ThreadPool() {
	if (waiterThread.joinable())
		waiterThread.join();
	destroyThreads();
	delete sharedHash;
}

void ThreadPool::threadLoop(SearchThread* st) {
	while (true) {
		{
			std::unique_lock<std::mutex> lock(st->mtx);
			st->cv.wait(lock, [st]{ return st->startFlag || st->exitFlag; });
			if (st->exitFlag) return;
			st->startFlag = false;
		}
		st->search.search(false);
		st->searching.store(false, std::memory_order_release);
	}
}

void ThreadPool::launchThreads() {
	for (int i = 0; i < numThreads; i++) {
		SearchThread* st = new SearchThread();
		st->threadId = i;
		st->search.threadId = i;
		st->search.sharedStop = &stopSignal;
		threads.push_back(st);
		st->nativeThread = std::thread(&ThreadPool::threadLoop, this, st);
	}
}

void ThreadPool::destroyThreads() {
	for (auto* st : threads) {
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->exitFlag = true;
		}
		st->cv.notify_one();
		if (st->nativeThread.joinable())
			st->nativeThread.join();
		delete st;
	}
	threads.clear();
}

void ThreadPool::setThreadCount(int n) {
	if (n < 1) n = 1;
	if (n > 64) n = 64;
	if (n == numThreads) return;

	if (waiterThread.joinable())
		waiterThread.join();
	destroyThreads();
	numThreads = n;
	launchThreads();
}

int ThreadPool::selectBestMove() {
	if (numThreads == 1) {
		return threads[0]->search.rootBestMove;
	}

	// Find minimum score across all threads that completed at least depth 1
	int minScore = Search::INFINITE;
	for (auto* st : threads) {
		if (st->search.completedDepth > 0 && st->search.rootBestScore < minScore) {
			minScore = st->search.rootBestScore;
		}
	}

	// Vote: each thread votes for its bestmove with weight = (score - minScore + 14) * completedDepth
	// Index by from-to (from*64 + to) to group identical moves
	std::unordered_map<int, int> votes;       // moveKey -> total vote
	std::unordered_map<int, int> moveForKey;  // moveKey -> actual move int

	for (auto* st : threads) {
		int mv = st->search.rootBestMove;
		if (mv == Move::NO_MOVE || st->search.completedDepth == 0)
			continue;

		int key = Move::from(mv) * 64 + Move::to(mv);
		int vote = (st->search.rootBestScore - minScore + 14) * st->search.completedDepth;
		votes[key] += vote;
		// Keep the move from the deepest thread for this key
		if (moveForKey.find(key) == moveForKey.end() || st->search.completedDepth > 0) {
			moveForKey[key] = mv;
		}
	}

	// Find move with highest total vote
	int bestKey = -1;
	int bestVote = -1;
	for (auto& kv : votes) {
		if (kv.second > bestVote) {
			bestVote = kv.second;
			bestKey = kv.first;
		}
	}

	if (bestKey >= 0 && moveForKey.count(bestKey)) {
		return moveForKey[bestKey];
	}

	// Fallback: thread 0's result
	return threads[0]->search.rootBestMove;
}

void ThreadPool::startSearch(Board& rootBoard, SearchInfo& info, bool verbose) {
	// Join previous waiter if still running
	if (waiterThread.joinable())
		waiterThread.join();

	stopSignal.store(false);
	bestMove = 0;
	verboseSearch = verbose;

	for (auto* st : threads) {
		// Mark searching BEFORE signaling — prevents waitForSearch race
		st->searching.store(true, std::memory_order_release);

		// Each thread gets its own copy of the board
		st->search.board = rootBoard;
		// But all share the same hash table
		st->search.board.setHashTable(sharedHash);
		st->search.info = info;
		st->search.sharedStop = &stopSignal;

		// Only main thread manages time; helper threads rely on shared stop
		if (st->threadId != 0) {
			st->search.info.timeSet = false;
		}

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->startFlag = true;
		}
		st->cv.notify_one();
	}

	// Spawn waiter thread to handle completion, voting, and bestmove output
	waiterThread = std::thread(&ThreadPool::waiterFunc, this);
}

void ThreadPool::stopSearch() {
	stopSignal.store(true, std::memory_order_relaxed);
	// Also set info.stopped on all threads
	for (auto* st : threads) {
		st->search.info.stopped = true;
	}
}

void ThreadPool::waiterFunc() {
	// Wait for all threads to finish
	for (auto* st : threads) {
		while (st->searching.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
	}

	// Select best move via voting
	bestMove = selectBestMove();

	// Print bestmove if verbose (UCI flow)
	if (verboseSearch && bestMove != Move::NO_MOVE) {
		std::cout << "bestmove " << Move::toLongNotation(bestMove) << std::endl;
	}
}

void ThreadPool::waitForSearch() {
	// Join the waiter thread (blocks until voting + bestmove output complete)
	if (waiterThread.joinable())
		waiterThread.join();
}
