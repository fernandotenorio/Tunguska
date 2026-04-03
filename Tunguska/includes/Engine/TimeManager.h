#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <chrono>
#include <algorithm>

class TimeManager {
private:
    long long optimumTime;
    long long maximumTime;
    long long startTime;
    bool isInfinite;
    double timeMultiplier;

public:
    TimeManager() : optimumTime(-1), maximumTime(-1), startTime(0), isInfinite(false), timeMultiplier(1.0) {}

    void init(long long wtime, long long btime, long long winc, long long binc, int movestogo, bool isWhite, long long explicitMovetime, bool infinite) {
        isInfinite = infinite;
        timeMultiplier = 1.0;

        if (isInfinite) {
            optimumTime = -1;
            maximumTime = -1;
            return;
        }

        if (explicitMovetime != -1) {
            optimumTime = explicitMovetime;
            maximumTime = explicitMovetime;
            return;
        }

        long long timeLeft = isWhite ? wtime : btime;
        long long inc = isWhite ? winc : binc;

        if (timeLeft <= 0) {
            optimumTime = 1;
            maximumTime = 1;
            return;
        }

        // Engine overhead to avoid flagging in fast/bullet time controls
        long long moveOverhead = 10; 
        timeLeft = std::max(1LL, timeLeft - moveOverhead);

        // Estimate moves remaining based on time control type
        int movesRemaining = (movestogo > 0) ? std::min(movestogo, 50) : 40;

        // Base time allocation calculation
        double baseTime = (double)timeLeft / movesRemaining;
        
        // Calculate optimum and maximum times 
        optimumTime = static_cast<long long>(baseTime + inc * 0.75);
        maximumTime = static_cast<long long>(baseTime * 5.0 + inc * 0.75);

        // Safety cap: never use more than a fraction of the total time left for optimum,
        // and strictly bounded by total time left for maximum.
        optimumTime = std::min(optimumTime, static_cast<long long>(timeLeft * 0.5));
        maximumTime = std::min(maximumTime, static_cast<long long>(timeLeft * 0.8));

        // Ensure strict optimum <= maximum bound
        optimumTime = std::max(1LL, std::min(optimumTime, maximumTime));
    }

    void start() {
        startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    // Extends the soft limit (optimum time) if the search detects instability
    void extendTime() {
        timeMultiplier = std::min(2.0, timeMultiplier * 1.5);
    }

    long long elapsed() const {
        long long current = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        return current - startTime;
    }

    bool checkHardLimit() const {
        if (isInfinite || maximumTime == -1) return false;
        return elapsed() >= maximumTime;
    }

    bool checkSoftLimit() const {
        if (isInfinite || optimumTime == -1) return false;
        long long currentSoftLimit = static_cast<long long>(optimumTime * timeMultiplier);
        currentSoftLimit = std::min(currentSoftLimit, maximumTime); // Hard limit takes priority
        return elapsed() >= currentSoftLimit;
    }
};

#endif