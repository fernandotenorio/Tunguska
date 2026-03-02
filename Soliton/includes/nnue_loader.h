#ifndef NNUELoader_H
#define NNUELoader_H

#include <Eigen/Dense>
#include <string>
#include "NNUEConstants.h"
#include "FeatureExtractor.h"

class Board; // Forward declaration of Board

struct NNUEAccumulator {
    float v[2][HL_SIZE];

    float* operator[](Side stm) {
        return v[stm];
    }
};

class NNUELoader {
private:
    NNUELoader(const NNUELoader&) = delete; // Delete copy constructor
    NNUELoader& operator=(const NNUELoader&) = delete; // Delete assignment operator
    static NNUELoader* instance;
public:
    Eigen::MatrixXf accumulator_weight;  // Dynamic allocation for accumulator weights
    Eigen::VectorXf output_weights;      // Dynamic allocation for output weights
    Eigen::VectorXf accumulator_bias;    // Bias for the accumulators
    float output_bias;                   // Final output bias

    bool weights_loaded;
    StartingFeatures initialFeatures;   //avoid allocating vectors when position set in UCI: positions startpos moves ...

    NNUEAccumulator accumulator;         // Stores the current accumulator state
    Eigen::VectorXf combined_accumulator; // Now a member variable
    NNUELoader() {}

    static NNUELoader& getInstance() {
        if (!instance) {
            instance = new NNUELoader();
        }
        return *instance;
    }

    //Destructor
    ~NNUELoader() {}

    // Load weights from a file
    void loadWeights(const std::string& filename);

    // Perform forward pass
    float forward(Eigen::VectorXf x_white, Eigen::VectorXf x_black, Side stm);


    void setAccumulator(NNUEAccumulator& new_acc, const std::vector<int>& active_features, Side stm);
    void setAccumulator(NNUEAccumulator& new_acc, const std::vector<int>& active_idxs, int cnt, Side stm);
    void setAccumulator(const Board& board);
    void updateAccumulator(const FeatureChanges& changes);
    void updateAccumulatorUndo(const FeatureChanges& changes);

  
    float computeOutput(Side stm);
    
};

#endif // NNUE_H
