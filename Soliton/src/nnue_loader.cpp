#include "nnue_loader.h"
#include "Engine/Board.h"
#include "FeatureExtractor.h"
#include "cnpy.h"
#include <iostream>
#include <algorithm> // for std::clamp
#include <cstring>   // for std::memcpy

// =========================================================
// 1. NNUENetwork (Static Weights)
// =========================================================
int16_t NNUENetwork::accumulator_weight[INPUT_SIZE][HL_SIZE];
int16_t NNUENetwork::accumulator_bias[HL_SIZE];
int16_t NNUENetwork::output_weights[2 * HL_SIZE];
int32_t NNUENetwork::output_bias = 0;
bool NNUENetwork::weights_loaded = false;

void NNUENetwork::loadWeights(const std::string& filename) {
    if (weights_loaded) return;

    cnpy::npz_t npz = cnpy::npz_load(filename);

    // Fetch the quantized integer arrays
    int16_t* acc_w = npz["accumulator.weight"].data<int16_t>();
    int16_t* acc_b = npz["accumulator.bias"].data<int16_t>();
    int16_t* out_w = npz["output_weights"].data<int16_t>();
    int32_t* out_b = npz["output_bias"].data<int32_t>();

    // Copy directly into fast 1D/2D C-arrays
    std::memcpy(accumulator_weight, acc_w, INPUT_SIZE * HL_SIZE * sizeof(int16_t));
    std::memcpy(accumulator_bias, acc_b, HL_SIZE * sizeof(int16_t));
    std::memcpy(output_weights, out_w, 2 * HL_SIZE * sizeof(int16_t));
    output_bias = out_b[0];

    weights_loaded = true;
    std::cout << "Quantized INT16/INT32 NNUE Weights loaded." << std::endl;
}

// =========================================================
// 2. NNUEState (Thread-Local Calculation)
// =========================================================

void NNUEState::init(const Board& board) {
    if (!NNUENetwork::weights_loaded) return;

    StartingFeatures initialFeatures;
    FeatureExtractor::extractFeatures(board, initialFeatures);

    // 1. Start with Biases
    std::memcpy(accumulator.v[WHITE_NNUE], NNUENetwork::accumulator_bias, HL_SIZE * sizeof(int16_t));
    std::memcpy(accumulator.v[BLACK_NNUE], NNUENetwork::accumulator_bias, HL_SIZE * sizeof(int16_t));

    // 2. Add White Features
    for (int i = 0; i < initialFeatures.white_feats_cnt; ++i) {
        int feat = initialFeatures.white_feats_idx[i];
        for (int j = 0; j < HL_SIZE; ++j) {
            accumulator.v[WHITE_NNUE][j] += NNUENetwork::accumulator_weight[feat][j];
        }
    }

    // 3. Add Black Features
    for (int i = 0; i < initialFeatures.black_feats_cnt; ++i) {
        int feat = initialFeatures.black_feats_idx[i];
        for (int j = 0; j < HL_SIZE; ++j) {
            accumulator.v[BLACK_NNUE][j] += NNUENetwork::accumulator_weight[feat][j];
        }
    }
}

void NNUEState::update(const FeatureChanges& changes) {
    // --- WHITE ACCUMULATOR ---
    for (int i = 0; i < changes.rem_white_count; ++i) {
        int feat = changes.rem_white[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[WHITE_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] -= w[j]; }
    }
    for (int i = 0; i < changes.add_white_count; ++i) {
        int feat = changes.add_white[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[WHITE_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] += w[j]; }
    }

    // --- BLACK ACCUMULATOR ---
    for (int i = 0; i < changes.rem_black_count; ++i) {
        int feat = changes.rem_black[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[BLACK_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] -= w[j]; }
    }
    for (int i = 0; i < changes.add_black_count; ++i) {
        int feat = changes.add_black[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[BLACK_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] += w[j]; }
    }
}

void NNUEState::updateUndo(const FeatureChanges& changes) {
    // --- WHITE ACCUMULATOR ---
    for (int i = 0; i < changes.rem_white_count; ++i) {
        int feat = changes.rem_white[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[WHITE_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] += w[j]; }
    }
    for (int i = 0; i < changes.add_white_count; ++i) {
        int feat = changes.add_white[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[WHITE_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] -= w[j]; }
    }

    // --- BLACK ACCUMULATOR ---
    for (int i = 0; i < changes.rem_black_count; ++i) {
        int feat = changes.rem_black[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[BLACK_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] += w[j]; }
    }
    for (int i = 0; i < changes.add_black_count; ++i) {
        int feat = changes.add_black[i];
        const int16_t* w = NNUENetwork::accumulator_weight[feat];
        int16_t* a = accumulator.v[BLACK_NNUE];
        for (int j = 0; j < HL_SIZE; ++j) { a[j] -= w[j]; }
    }
}

int NNUEState::evaluate(Side stm) {
    int32_t sum = NNUENetwork::output_bias;
    Side them = static_cast<Side>(1 - stm);

    const int16_t* us_acc = accumulator.v[stm];
    const int16_t* them_acc = accumulator.v[them];
    const int16_t* out_w_us = NNUENetwork::output_weights;
    const int16_t* out_w_them = NNUENetwork::output_weights + HL_SIZE;

    // A single, tightly packed loop. 
    // The compiler will turn this into the blazing fast `vpmaddwd` instruction!
    for (int i = 0; i < HL_SIZE; ++i) {
        int16_t act_us = std::max<int16_t>(0, std::min<int16_t>(QA, us_acc[i]));
        sum += static_cast<int32_t>(act_us) * out_w_us[i];

        int16_t act_them = std::max<int16_t>(0, std::min<int16_t>(QA, them_acc[i]));
        sum += static_cast<int32_t>(act_them) * out_w_them[i];
    }

    return sum / QA;
}