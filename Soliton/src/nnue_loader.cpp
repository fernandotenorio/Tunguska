#include "nnue_loader.h"
#include "Engine/Board.h"
#include "FeatureExtractor.h"
#include "cnpy.h"
#include <iostream>
#include <algorithm> // for std::clamp
#include <cstring>   // for std::memcpy
#include <immintrin.h>

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
    Side them = static_cast<Side>(1 - stm);

    // Because we used alignas(64), we can safely use the ultra-fast ALIGNED load instructions
    const int16_t* us_acc = accumulator.v[stm];
    const int16_t* them_acc = accumulator.v[them];
    const int16_t* out_w_us = NNUENetwork::output_weights;
    const int16_t* out_w_them = NNUENetwork::output_weights + HL_SIZE;

    // Set up AVX2 vector constants (256-bit registers)
    __m256i zero = _mm256_setzero_si256();
    __m256i qa   = _mm256_set1_epi16(QA);
    __m256i sum256 = _mm256_setzero_si256();

    // We process 16 elements (32 bytes) per loop iteration
    for (int i = 0; i < HL_SIZE; i += 16) {
        // --- Process US ---
        // 1. Load 16 accumulator values into a register
        __m256i act_us = _mm256_load_si256((const __m256i*)&us_acc[i]);
        // 2. Clamp between 0 and QA (16 at a time!)
        act_us = _mm256_max_epi16(act_us, zero);
        act_us = _mm256_min_epi16(act_us, qa);
        // 3. Load 16 output weights
        __m256i w_us = _mm256_load_si256((const __m256i*)&out_w_us[i]);
        // 4. Multiply and Add adjacent pairs (The magical vpmaddwd instruction!)
        __m256i prod_us = _mm256_madd_epi16(act_us, w_us);
        // 5. Accumulate into 32-bit sums
        sum256 = _mm256_add_epi32(sum256, prod_us);

        // --- Process THEM ---
        __m256i act_them = _mm256_load_si256((const __m256i*)&them_acc[i]);
        act_them = _mm256_max_epi16(act_them, zero);
        act_them = _mm256_min_epi16(act_them, qa);
        
        __m256i w_them = _mm256_load_si256((const __m256i*)&out_w_them[i]);
        __m256i prod_them = _mm256_madd_epi16(act_them, w_them);
        sum256 = _mm256_add_epi32(sum256, prod_them);
    }

    // --- Horizontal Sum ---
    // At this point, sum256 holds EIGHT separate 32-bit integers. 
    // We need to fold them all together into a single standard int.
    
    // 1. Extract the upper 128 bits and add to the lower 128 bits
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum256), _mm256_extracti128_si256(sum256, 1));
    
    // 2. Shuffle and add (reduces 4 ints to 2 ints)
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2)));
    
    // 3. Shuffle and add (reduces 2 ints to 1 int)
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1)));
    
    // 4. Extract the final single integer from the register
    int final_sum = _mm_cvtsi128_si32(sum128);

    // Add the bias and scale back down
    return (final_sum + NNUENetwork::output_bias) / QA;
}