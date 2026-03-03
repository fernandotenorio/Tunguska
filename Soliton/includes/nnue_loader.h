#ifndef NNUELOADER_H
#define NNUELOADER_H

#include <Eigen/Dense>
#include <string>
#include <vector>
#include "NNUEConstants.h"
//#include "FeatureExtractor.h"

// Forward declaration
class Board;
struct FeatureChanges; 
struct StartingFeatures;

// 1. The Accumulator Structure (Stateful)
struct NNUEAccumulator {
    float v[2][HL_SIZE]; // White and Black accumulators

    float* operator[](int stm) {
        return v[stm];
    }
    const float* operator[](int stm) const {
        return v[stm];
    }
};

// 2. The Network Weights (Stateless, Read-Only, Shared)
class NNUENetwork {
public:
    static Eigen::MatrixXf accumulator_weight;
    static Eigen::VectorXf output_weights;
    static Eigen::VectorXf accumulator_bias;
    static float output_bias;
    static bool weights_loaded;

    static void loadWeights(const std::string& filename);
};

// 3. The Computation State (Thread-Local)
class NNUEState {
public:
    NNUEAccumulator accumulator;
    Eigen::VectorXf combined_accumulator;
    //StartingFeatures initialFeatures;

    NNUEState();
    
    // Reset accumulator from scratch (for root position or new game)
    void init(const Board& board);

    // Update accumulator incrementally (make move)
    void update(const FeatureChanges& changes);
    
    // Update accumulator incrementally (undo move)
    void updateUndo(const FeatureChanges& changes);

    // Calculate final score
    float evaluate(Side stm);
};

#endif // NNUELOADER_H