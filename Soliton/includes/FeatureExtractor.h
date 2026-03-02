#pragma once
#include <vector>
#include <string>
#include <cstdint> // For int32_t
#include <cassert>
#include <sstream>
#include <iostream>
#include <utility> // For std::pair
#include "Engine/Board.h"
#include "Engine/defs.h"
#include "NNUEConstants.h"

struct StartingFeatures {
    int white_feats_cnt;
    int black_feats_cnt;
    std::vector<int> white_feats_idx;
    std::vector<int> black_feats_idx;

    StartingFeatures() : 
        white_feats_cnt(0),
        black_feats_cnt(0),
        white_feats_idx(32),
        black_feats_idx(32) {}

    void add_white_feat(int idx) {
        white_feats_idx[white_feats_cnt++] = idx;
    }
    
    void add_black_feat(int idx) {
        black_feats_idx[black_feats_cnt++] = idx;
    }

    void reset(){
        white_feats_cnt = 0;
        black_feats_cnt = 0;
    }
};


struct FeatureChanges {
    int add_white_count;
    int rem_white_count;
    std::vector<int> add_white;
    std::vector<int> rem_white;
    int add_black_count;
    int rem_black_count;
    std::vector<int> add_black;
    std::vector<int> rem_black;

    FeatureChanges() :
        add_white_count(0),
        rem_white_count(0),
        add_white(4),
        rem_white(4),
        add_black_count(0),
        rem_black_count(0),
        add_black(4),
        rem_black(4) {}

    void add_white_feat(int idx) {
        add_white[add_white_count++] = idx;
    }
    void rem_white_feat(int idx) {
        rem_white[rem_white_count++] = idx;
    }

    void add_black_feat(int idx) {
        add_black[add_black_count++] = idx;
    }
    void rem_black_feat(int idx) {
        rem_black[rem_black_count++] = idx;
    }
};

class FeatureExtractor {
public:
    static std::pair<std::vector<int>, std::vector<int>> extractFeatures(const Board& board);
    static void extractFeatures(const Board& board, StartingFeatures& initialFeatures);
    static std::pair<std::vector<int>, std::vector<int>> extractFeatures(const std::string& fen);
    static FeatureChanges moveDiffFeatures(const Board& board, int move);

private:
    // No member variables needed
    static void addFeature(std::vector<int>& features, int feature_index) {
        features[feature_index] = 1;
    }
};