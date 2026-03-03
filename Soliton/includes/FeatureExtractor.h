#pragma once
#include <cstdint>
#include <utility>
#include "Engine/Board.h"
#include "Engine/defs.h"
#include "NNUEConstants.h"

// Max pieces on board = 32. Fixed size array prevents heap allocation.
struct StartingFeatures {
    int white_feats_cnt;
    int black_feats_cnt;
    int white_feats_idx[32];
    int black_feats_idx[32];

    StartingFeatures() : 
        white_feats_cnt(0),
        black_feats_cnt(0) {}

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

// Max changes per move is usually 2 (move from -> to). 
// Castling involves 4 changes (King move + Rook move). 
// Size 4 is sufficient.
struct FeatureChanges {
    int add_white_count;
    int rem_white_count;
    int add_white[4];
    int rem_white[4];

    int add_black_count;
    int rem_black_count;
    int add_black[4];
    int rem_black[4];

    FeatureChanges() :
        add_white_count(0),
        rem_white_count(0),
        add_black_count(0),
        rem_black_count(0) {}

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
    // Used for debugging or Python binding (returns heap vectors)
    static std::pair<std::vector<int>, std::vector<int>> extractFeatures(const Board& board);
    static std::pair<std::vector<int>, std::vector<int>> extractFeatures(const std::string& fen);

    // Used for Engine Search (Optimized, no heap alloc)
    static void extractFeatures(const Board& board, StartingFeatures& initialFeatures);
    static FeatureChanges moveDiffFeatures(const Board& board, int move);

private:
    static void addFeature(std::vector<int>& features, int feature_index) {
        features[feature_index] = 1;
    }
};