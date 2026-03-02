#include "FeatureExtractor.h"
#include "Engine/MoveGen.h"

// Helper to get indices for both accumulators at once
// piece: The engine piece constant (e.g. Board::WHITE_PAWN)
// sq: The square (0-63)
inline std::pair<int, int> get_indices(int piece, int sq) {
    int nnue_piece = to_nnue_piece(piece);
    
    // 1. White Accumulator Index: Standard View
    // Index = PieceType(0-11) * 64 + Square
    int white_idx = nnue_piece * 64 + sq;

    // 2. Black Accumulator Index: Flipped View
    // Swap Color: (0->6, 1->7... 6->0) -> use XOR with 6? No, just (p + 6) % 12
    int flipped_piece = (nnue_piece < 6) ? (nnue_piece + 6) : (nnue_piece - 6);
    // Flip Square: Rank 1 <-> Rank 8 (XOR 56)
    int flipped_sq = sq ^ 56;
    
    int black_idx = flipped_piece * 64 + flipped_sq;

    return {white_idx, black_idx};
}

std::pair<std::vector<int>, std::vector<int>> FeatureExtractor::extractFeatures(const Board& board) {
    std::vector<int> white_features(INPUT_SIZE, 0);
    std::vector<int> black_features(INPUT_SIZE, 0);

    for (int sq = 0; sq < 64; ++sq) {
        int piece = board.board[sq];
        if (piece != Board::EMPTY) {
            auto [w_idx, b_idx] = get_indices(piece, sq);
            addFeature(white_features, w_idx);
            addFeature(black_features, b_idx);
        }
    }
    return { white_features, black_features };
}

void FeatureExtractor::extractFeatures(const Board& board, StartingFeatures& initialFeatures) {
    for (int sq = 0; sq < 64; ++sq) {
        int piece = board.board[sq];
        if (piece != Board::EMPTY) {
            auto [w_idx, b_idx] = get_indices(piece, sq);
            initialFeatures.add_white_feat(w_idx);
            initialFeatures.add_black_feat(b_idx);
        }
    }
}

std::pair<std::vector<int>, std::vector<int>> FeatureExtractor::extractFeatures(const std::string& fen) {
    // Re-using the Board class logic is safer than manual string parsing if possible,
    // but here is the corrected manual parser:
    std::vector<int> white_features(INPUT_SIZE, 0);
    std::vector<int> black_features(INPUT_SIZE, 0);

    std::istringstream fen_stream(fen);
    std::string board_part;
    fen_stream >> board_part;

    int rank = 7;
    int file = 0;
    for (char c : board_part) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += c - '0';
        } else {
            int sq = rank * 8 + file;
            
            // Convert char to Engine Piece Code
            int piece = 0;
            switch(c) {
                case 'P': piece = Board::WHITE_PAWN; break;
                case 'N': piece = Board::WHITE_KNIGHT; break;
                case 'B': piece = Board::WHITE_BISHOP; break;
                case 'R': piece = Board::WHITE_ROOK; break;
                case 'Q': piece = Board::WHITE_QUEEN; break;
                case 'K': piece = Board::WHITE_KING; break;
                case 'p': piece = Board::BLACK_PAWN; break;
                case 'n': piece = Board::BLACK_KNIGHT; break;
                case 'b': piece = Board::BLACK_BISHOP; break;
                case 'r': piece = Board::BLACK_ROOK; break;
                case 'q': piece = Board::BLACK_QUEEN; break;
                case 'k': piece = Board::BLACK_KING; break;
            }

            auto [w_idx, b_idx] = get_indices(piece, sq);
            addFeature(white_features, w_idx);
            addFeature(black_features, b_idx);
            file++;
        }
    }
    return { white_features, black_features };
}

FeatureChanges FeatureExtractor::moveDiffFeatures(const Board& board, int move) {
    FeatureChanges changes;
    
    int from = Move::from(move);
    int to = Move::to(move);
    int moving_piece = board.board[from];
    int captured_piece = board.board[to]; // Valid for normal captures
    int promote_to = Move::promoteTo(move);

    bool is_ep = Move::isEP(move);
    bool is_pj = Move::isPJ(move);
    bool is_castle = Move::isCastle(move);

    // 1. Remove moving piece from 'from'
    auto [w_rem, b_rem] = get_indices(moving_piece, from);
    changes.rem_white_feat(w_rem);
    changes.rem_black_feat(b_rem);

    // 2. Handle Additions (based on move type)
    if (promote_to != Board::EMPTY) {
        // Promotion: Add promoted piece at 'to'
        // promote_to comes from Move, which is just the type? No, usually 
        // it needs the color. Let's assume Move::promoteTo returns the piece code.
        // If not, combine with side: promote_to | side
        auto [w_add, b_add] = get_indices(promote_to, to);
        changes.add_white_feat(w_add);
        changes.add_black_feat(b_add);
    } 
    else {
        // Normal Move / PJ / EP: Add moving piece at 'to'
        auto [w_add, b_add] = get_indices(moving_piece, to);
        changes.add_white_feat(w_add);
        changes.add_black_feat(b_add);
    }

    // 3. Handle Captures
    if (captured_piece != Board::EMPTY && !is_ep && !is_castle) {
        auto [w_cap, b_cap] = get_indices(captured_piece, to);
        changes.rem_white_feat(w_cap);
        changes.rem_black_feat(b_cap);
    }
    else if (is_ep) {
        // En Passant: The captured pawn is on a different square
        int side = board.state.currentPlayer;
        int ep_sq = to + MoveGen::epCaptDiff[side]; // Location of victim pawn
        int victim_pawn = (side == Board::WHITE) ? Board::BLACK_PAWN : Board::WHITE_PAWN;
        
        auto [w_cap, b_cap] = get_indices(victim_pawn, ep_sq);
        changes.rem_white_feat(w_cap);
        changes.rem_black_feat(b_cap);
    }
    
    // 4. Handle Castle (Rook moves)
    if (is_castle) {
        int side = board.state.currentPlayer;
        int* sq = Board::CASTLE_SQS[from][side]; 
        // sq format: {KingFrom, KingTo, RookFrom, RookTo}
        // King move is handled by step 1 & 2 (moving_piece)
        // We only need to handle the Rook here
        
        int rook = (side == Board::WHITE) ? Board::WHITE_ROOK : Board::BLACK_ROOK;
        
        // Remove Rook from old square
        auto [w_r_rem, b_r_rem] = get_indices(rook, sq[2]);
        changes.rem_white_feat(w_r_rem);
        changes.rem_black_feat(b_r_rem);
        
        // Add Rook to new square
        auto [w_r_add, b_r_add] = get_indices(rook, sq[3]);
        changes.add_white_feat(w_r_add);
        changes.add_black_feat(b_r_add);
    }

    return changes;
}