#include "nnue_loader.h"
#include "cnpy.h"
#include <Eigen/Dense>
#include <Engine/Board.h>

NNUELoader* NNUELoader::instance = nullptr; // Initialize the static instance


void NNUELoader::loadWeights(const std::string& filename) {
    if (weights_loaded) {
        return;
    }
    cnpy::npz_t npz = cnpy::npz_load(filename);
    //std::cout << npz["accumulator.bias"].shape[0] << std::endl;

    using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    accumulator_weight = Eigen::Map<RowMajorMatrixXf>(npz["accumulator.weight"].data<float>(), HL_SIZE, INPUT_SIZE);
    output_weights = Eigen::Map<Eigen::VectorXf>(npz["output_weights"].data<float>(), 2 * HL_SIZE);
    accumulator_bias = Eigen::Map<Eigen::VectorXf>(npz["accumulator.bias"].data<float>(), HL_SIZE);
    output_bias = npz["output_bias"].data<float>()[0];

    // Initialize combined_accumulator
    combined_accumulator.resize(2 * accumulator_bias.size());
    combined_accumulator.setZero();
    std::cout << "NNUE Weights loaded." << std::endl;
    weights_loaded = true;
}

//active_features is a INPUT_SIZE binary vector
void NNUELoader::setAccumulator(NNUEAccumulator& new_acc, const std::vector<int>& active_features, Side stm) {
    // Initialize with bias
    Eigen::VectorXf acc = accumulator_bias;

    for (size_t idx = 0; idx < active_features.size(); idx++) {
        if (active_features[idx]) {
            acc += accumulator_weight.col(idx);
        }
    }
    std::memcpy(new_acc[stm], acc.data(), HL_SIZE * sizeof(float));
}

//active_idxs holds sequence of idxs
void NNUELoader::setAccumulator(NNUEAccumulator& new_acc, const std::vector<int>& active_idxs, int cnt, Side stm) {
    // Initialize with bias
    Eigen::VectorXf acc = accumulator_bias;

    for (size_t i = 0; i < cnt; i++) {
        acc += accumulator_weight.col(active_idxs[i]);
    }
    std::memcpy(new_acc[stm], acc.data(), HL_SIZE * sizeof(float));
}

void NNUELoader::setAccumulator(const Board& board) {    
    //auto [white_features, black_features] = FeatureExtractor::extractFeatures(board);
    initialFeatures.reset();
    FeatureExtractor::extractFeatures(board, initialFeatures);
    this->setAccumulator(this->accumulator, initialFeatures.white_feats_idx, initialFeatures.white_feats_cnt, WHITE_NNUE);
    this->setAccumulator(this->accumulator, initialFeatures.black_feats_idx, initialFeatures.black_feats_cnt, BLACK_NNUE);
    //this->setAccumulator(this->accumulator, white_features, WHITE_NNUE);
    //this->setAccumulator(this->accumulator, black_features, BLACK_NNUE);
}


void NNUELoader::updateAccumulator(const FeatureChanges& changes)
{
    //white
    Eigen::Map<Eigen::VectorXf> acc_w(accumulator[WHITE_NNUE], HL_SIZE);
    for (size_t i = 0; i < changes.add_white_count; i++) {
        acc_w += accumulator_weight.col(changes.add_white[i]);
    }
    for (size_t i = 0; i < changes.rem_white_count; i++) {
        acc_w -= accumulator_weight.col(changes.rem_white[i]);
    }

    //black
    Eigen::Map<Eigen::VectorXf> acc_b(accumulator[BLACK_NNUE], HL_SIZE);
    for (size_t i = 0; i < changes.add_black_count; i++) {
        acc_b += accumulator_weight.col(changes.add_black[i]);
    }
    for (size_t i = 0; i < changes.rem_black_count; i++) {
        acc_b -= accumulator_weight.col(changes.rem_black[i]);
    }
}

void NNUELoader::updateAccumulatorUndo(const FeatureChanges& changes)
{
    //white
    Eigen::Map<Eigen::VectorXf> acc_w(accumulator[WHITE_NNUE], HL_SIZE);
    for (size_t i = 0; i < changes.add_white_count; i++) {
        acc_w -= accumulator_weight.col(changes.add_white[i]);
    }
    for (size_t i = 0; i < changes.rem_white_count; i++) {
        acc_w += accumulator_weight.col(changes.rem_white[i]);
    }

    //black
    Eigen::Map<Eigen::VectorXf> acc_b(accumulator[BLACK_NNUE], HL_SIZE);
    for (size_t i = 0; i < changes.add_black_count; i++) {
        acc_b -= accumulator_weight.col(changes.add_black[i]);
    }
    for (size_t i = 0; i < changes.rem_black_count; i++) {
        acc_b += accumulator_weight.col(changes.rem_black[i]);
    }
}


float NNUELoader::computeOutput(Side stm) {
    combined_accumulator.head(HL_SIZE) = Eigen::Map<const Eigen::VectorXf>(accumulator[stm], HL_SIZE);
    combined_accumulator.tail(HL_SIZE) = Eigen::Map<const Eigen::VectorXf>(accumulator[static_cast<Side>(1 - stm)], HL_SIZE);

    float eval_raw = combined_accumulator.dot(output_weights) + output_bias;
    return eval_raw * SCALE;
}


float NNUELoader::forward(Eigen::VectorXf x_white, Eigen::VectorXf x_black, Side stm) {
    // Compute accumulator for both white and black pieces
    Eigen::VectorXf white_accumulator = ((accumulator_weight * x_white) + accumulator_bias).cwiseMax(0).cwiseMin(QA);
    Eigen::VectorXf black_accumulator = ((accumulator_weight * x_black) + accumulator_bias).cwiseMax(0).cwiseMin(QA);

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
    return eval_raw * SCALE / (QA * QB);
}
