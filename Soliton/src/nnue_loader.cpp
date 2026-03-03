#include "nnue_loader.h"
#include "Engine/Board.h"
#include "FeatureExtractor.h"
#include "cnpy.h"
#include <iostream>



// =========================================================
// 1. NNUENetwork (Static Weights)
// =========================================================
Eigen::MatrixXf NNUENetwork::accumulator_weight;
Eigen::VectorXf NNUENetwork::output_weights;
Eigen::VectorXf NNUENetwork::accumulator_bias;
float NNUENetwork::output_bias = 0.0f;
bool NNUENetwork::weights_loaded = false;

void NNUENetwork::loadWeights(const std::string& filename) {
    if (weights_loaded) {
        return;
    }
    cnpy::npz_t npz = cnpy::npz_load(filename);

    using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    accumulator_weight = Eigen::Map<RowMajorMatrixXf>(npz["accumulator.weight"].data<float>(), HL_SIZE, INPUT_SIZE);
    output_weights = Eigen::Map<Eigen::VectorXf>(npz["output_weights"].data<float>(), 2 * HL_SIZE);
    accumulator_bias = Eigen::Map<Eigen::VectorXf>(npz["accumulator.bias"].data<float>(), HL_SIZE);
    output_bias = npz["output_bias"].data<float>()[0];
    weights_loaded = true;
    std::cout << "NNUE Weights loaded." << std::endl;
}


// =========================================================
// 2. NNUEState (Thread-Local Calculation)
// =========================================================

NNUEState::NNUEState() {
    combined_accumulator.resize(2 * HL_SIZE);
    combined_accumulator.setZero();
}

void NNUEState::init(const Board& board) {
    if (!NNUENetwork::weights_loaded) return;
    
    // Use stack-allocated struct (No heap malloc!)
    StartingFeatures initialFeatures;
    FeatureExtractor::extractFeatures(board, initialFeatures);

    // Start with Bias
    Eigen::VectorXf acc_w = NNUENetwork::accumulator_bias;
    Eigen::VectorXf acc_b = NNUENetwork::accumulator_bias;

    // Add features (Iterate strictly up to count)
    for(int i = 0; i < initialFeatures.white_feats_cnt; ++i) {
        acc_w += NNUENetwork::accumulator_weight.col(initialFeatures.white_feats_idx[i]);
    }

    for(int i = 0; i < initialFeatures.black_feats_cnt; ++i) {
        acc_b += NNUENetwork::accumulator_weight.col(initialFeatures.black_feats_idx[i]);
    }

    // Copy to persistent accumulator state
    std::memcpy(accumulator.v[WHITE_NNUE], acc_w.data(), HL_SIZE * sizeof(float));
    std::memcpy(accumulator.v[BLACK_NNUE], acc_b.data(), HL_SIZE * sizeof(float));
}

void NNUEState::update(const FeatureChanges& changes) {
    // Map existing accumulator memory to Eigen Vector for easy math
    Eigen::Map<Eigen::VectorXf> acc_w(accumulator.v[WHITE_NNUE], HL_SIZE);
    Eigen::Map<Eigen::VectorXf> acc_b(accumulator.v[BLACK_NNUE], HL_SIZE);

    // --- WHITE ACCUMULATOR ---
    // Remove old features
    for (int i = 0; i < changes.rem_white_count; ++i) {
        acc_w -= NNUENetwork::accumulator_weight.col(changes.rem_white[i]);
    }
    // Add new features
    for (int i = 0; i < changes.add_white_count; ++i) {
        acc_w += NNUENetwork::accumulator_weight.col(changes.add_white[i]);
    }

    // --- BLACK ACCUMULATOR ---
    // Remove old features
    for (int i = 0; i < changes.rem_black_count; ++i) {
        acc_b -= NNUENetwork::accumulator_weight.col(changes.rem_black[i]);
    }
    // Add new features
    for (int i = 0; i < changes.add_black_count; ++i) {
        acc_b += NNUENetwork::accumulator_weight.col(changes.add_black[i]);
    }
}

void NNUEState::updateUndo(const FeatureChanges& changes) {
    // Exact inverse of update
    Eigen::Map<Eigen::VectorXf> acc_w(accumulator.v[WHITE_NNUE], HL_SIZE);
    Eigen::Map<Eigen::VectorXf> acc_b(accumulator.v[BLACK_NNUE], HL_SIZE);

    // --- WHITE ACCUMULATOR ---
    // Undo Removal (Add back)
    for (int i = 0; i < changes.rem_white_count; ++i) {
        acc_w += NNUENetwork::accumulator_weight.col(changes.rem_white[i]);
    }
    // Undo Addition (Subtract)
    for (int i = 0; i < changes.add_white_count; ++i) {
        acc_w -= NNUENetwork::accumulator_weight.col(changes.add_white[i]);
    }

    // --- BLACK ACCUMULATOR ---
    // Undo Removal (Add back)
    for (int i = 0; i < changes.rem_black_count; ++i) {
        acc_b += NNUENetwork::accumulator_weight.col(changes.rem_black[i]);
    }
    // Undo Addition (Subtract)
    for (int i = 0; i < changes.add_black_count; ++i) {
        acc_b -= NNUENetwork::accumulator_weight.col(changes.add_black[i]);
    }
}

int NNUEState::evaluate(Side stm) {
    // 1. Load Accumulators for the current perspective
    // Note: cwiseMax(0.0) is ReLU. cwiseMin(1.0) clips it.
    
    // US (stm)
    combined_accumulator.head(HL_SIZE) = 
        Eigen::Map<const Eigen::VectorXf>(accumulator[stm], HL_SIZE)
        .cwiseMax(0.0f).cwiseMin(1.0f);

    // THEM (1-stm)
    combined_accumulator.tail(HL_SIZE) = 
        Eigen::Map<const Eigen::VectorXf>(accumulator[static_cast<Side>(1 - stm)], HL_SIZE)
        .cwiseMax(0.0f).cwiseMin(1.0f);

    // 2. Final Dot Product
    float eval_raw = combined_accumulator.dot(NNUENetwork::output_weights) + NNUENetwork::output_bias;
    return (int)(eval_raw * SCALE);
}

/*
float NNUELoader::forward(Eigen::VectorXf x_white, Eigen::VectorXf x_black, Side stm) {
    // Compute accumulator for both white and black pieces
    Eigen::VectorXf white_accumulator = ((accumulator_weight * x_white) + accumulator_bias).cwiseMax(0).cwiseMin(1);
    Eigen::VectorXf black_accumulator = ((accumulator_weight * x_black) + accumulator_bias).cwiseMax(0).cwiseMin(1);

    // Combine accumulators based on which side is to move
    Eigen::VectorXf combined_accumulator(2 * HL_SIZE);
    if (stm == BLACK_NNUE) {
        combined_accumulator << black_accumulator, white_accumulator;
    }
    else {
        combined_accumulator << white_accumulator, black_accumulator;
    }

    // Perform the dot product and apply bias
    float eval_raw = combined_accumulator.dot(output_weights) + output_bias;

    // Return the scaled result
    return eval_raw * SCALE;
}
*/