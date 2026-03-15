#ifndef NNUELOADER_H
#define NNUELOADER_H

#include <string>
#include <vector>
#include <cstdint>
#include "NNUEConstants.h"

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
    alignas(64) static int16_t accumulator_weight[INPUT_SIZE][HL_SIZE];
    alignas(64) static int16_t accumulator_bias[HL_SIZE];
    alignas(64) static int16_t output_weights[2 * HL_SIZE];
    static int32_t output_bias;
    static bool weights_loaded;

    static void loadWeights(const std::string& filename);
};

class NNUEState {
public:
    NNUEAccumulator accumulator;

    NNUEState() = default;
    
    void init(const Board& board);
    void update(const FeatureChanges& changes);
    void updateUndo(const FeatureChanges& changes);
    
    int evaluate(Side stm);
};

#endif // NNUELOADER_H