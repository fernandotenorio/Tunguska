#ifndef NNUELOADER_H
#define NNUELOADER_H

#include <string>
#include <vector>
#include <cstdint>
#include <cassert>
#include <memory>
#include "NNUE/NNUEConstants.h"

class Board;
struct FeatureChanges; 
struct StartingFeatures;

// 1. Aligned Accumulator
struct NNUEAccumulator {
    alignas(64) int16_t v[2][HL_SIZE]; 

    int16_t* operator[](int stm) { return v[stm]; }
    const int16_t* operator[](int stm) const { return v[stm]; }
};

// 2. Aligned Network Weights
class NNUENetwork {
public:
    alignas(64) static const int16_t accumulator_weight[INPUT_SIZE][HL_SIZE];
    alignas(64) static const int16_t accumulator_bias[HL_SIZE];
    alignas(64) static const int16_t output_weights[2 * HL_SIZE];
    static const int32_t output_bias;
    static bool weights_loaded;
};

class NNUEState {
private:
    // Heap-backed once per search thread; no allocation in update/pop.
    std::unique_ptr<NNUEAccumulator[]> accumulators;
    int accumulatorIndex = 0;

public:
    NNUEState();

    void init(const Board& board);
    // Push a fully computed child. Null moves share the current accumulator
    // and do not push/pop: the index counts real moves, not board.ply.
    void update(const FeatureChanges& changes);
    void pop() {
        assert(accumulatorIndex > 0);
        --accumulatorIndex;
    }
    const NNUEAccumulator& currentAccumulator() const {
        return accumulators[accumulatorIndex];
    }
    int evaluate(Side stm);
};

#endif // NNUELOADER_H
