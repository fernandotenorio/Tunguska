#include "FeatureExtractor.h"
#include "Engine/MoveGen.h" // For en passant square calculation
#include <iostream>

std::pair<std::vector<int>, std::vector<int>> FeatureExtractor::extractFeatures(const Board& board) {
    std::vector<int> white_features(INPUT_SIZE, 0); // Initialize with 0
    std::vector<int> black_features(INPUT_SIZE, 0); // Initialize with 0

    for (int piece_type = Board::PAWN; piece_type <= Board::KING; piece_type += 2) {
        for (int side = Board::WHITE; side <= Board::BLACK; side++) {
            int piece = piece_type | side;
            U64 bitboard = board.bitboards[piece];

            int side_nnue = 1 - side;
            int piece_type_nnue = (piece_type >> 1) - 1;

            while (bitboard) {
                int square = numberOfTrailingZeros(bitboard);
                int white_index = side_nnue * 64 * 6 + piece_type_nnue * 64 + square;
                int black_index = (1 - side_nnue) * 64 * 6 + piece_type_nnue * 64 + (square ^ 0b111000);
                addFeature(white_features, white_index);
                addFeature(black_features, black_index);
                bitboard &= bitboard - 1;
            }
        }
    }
    return { white_features, black_features };
}

void FeatureExtractor::extractFeatures(const Board& board, StartingFeatures& initialFeatures) {
    for (int piece_type = Board::PAWN; piece_type <= Board::KING; piece_type += 2) {
        for (int side = Board::WHITE; side <= Board::BLACK; side++) {
            int piece = piece_type | side;
            U64 bitboard = board.bitboards[piece];

            int side_nnue = 1 - side;
            int piece_type_nnue = (piece_type >> 1) - 1;

            while (bitboard) {
                int square = numberOfTrailingZeros(bitboard);
                int white_index = side_nnue * 64 * 6 + piece_type_nnue * 64 + square;
                int black_index = (1 - side_nnue) * 64 * 6 + piece_type_nnue * 64 + (square ^ 0b111000);               
                initialFeatures.add_white_feat(white_index);
                initialFeatures.add_black_feat(black_index);
                bitboard &= bitboard - 1;
            }
        }
    }
}

std::pair<std::vector<int>, std::vector<int>> FeatureExtractor::extractFeatures(const std::string& fen) {
    std::vector<int> white_features(INPUT_SIZE, 0);
    std::vector<int> black_features(INPUT_SIZE, 0);

    std::istringstream fen_stream(fen);
    std::string board_part, side_to_move_part, castling_part, en_passant_part, halfmove_clock_part, fullmove_number_part;
    fen_stream >> board_part >> side_to_move_part >> castling_part >> en_passant_part >> halfmove_clock_part >> fullmove_number_part;

    int rank = 7;
    int file = 0;
    for (char c : board_part) {
        if (c == '/') {
            rank--;
            file = 0;
        }
        else if (isdigit(c)) {
            file += c - '0';
        }
        else {
            Square square = rank * 8 + file;
            PieceType piece_type{};
            Side side = isupper(c) ? WHITE_NNUE : BLACK_NNUE;
            c = tolower(c);
            switch (c) {
            case 'p': piece_type = PAWN_NNUE;   break;
            case 'n': piece_type = KNIGHT_NNUE; break;
            case 'b': piece_type = BISHOP_NNUE; break;
            case 'r': piece_type = ROOK_NNUE;   break;
            case 'q': piece_type = QUEEN_NNUE;  break;
            case 'k': piece_type = KING_NNUE;   break;
            default:  assert(false); // Invalid FEN character
            }

            int white_index = side * 64 * 6 + piece_type * 64 + square;
            int black_index = (1 - side) * 64 * 6 + piece_type * 64 + (square ^ 0b111000);
            addFeature(white_features, white_index);
            addFeature(black_features, black_index);
            file++;
        }
    }
    return { white_features, black_features };
}

FeatureChanges FeatureExtractor::moveDiffFeatures(const Board& board, int move) {

    FeatureChanges changes;

    int side = board.state.currentPlayer;
    int opponent_side = side ^ 1;
    int from = Move::from(move);
    int to = Move::to(move);
    int capt = Move::captured(move);
    assert(capt != Board::WHITE_KING);
    assert(capt != Board::BLACK_KING);

    int moving_piece = board.board[from];
    int moving_piece_type = moving_piece - side;
    int moving_piece_nnue_type = (moving_piece_type >> 1) - 1;
    int promoteTo = Move::promoteTo(move);
    assert(promoteTo != Board::WHITE_KING);
    assert(promoteTo != Board::BLACK_KING);
    
    bool is_ep = Move::isEP(move);
    bool is_pj = Move::isPJ(move);
    bool is_castle = Move::isCastle(move);

    Side side_nnue = (side == Board::WHITE) ? WHITE_NNUE : BLACK_NNUE;

    if (is_pj) {
        int removed_white_index = side_nnue * 64 * 6 + moving_piece_nnue_type * 64 + from;
        int add_white_index = side_nnue * 64 * 6 + moving_piece_nnue_type * 64 + to;
        int removed_black_index = (1 - side_nnue) * 64 * 6 + moving_piece_nnue_type * 64 + (from ^ 0b111000);
        int add_black_index = (1 - side_nnue) * 64 * 6 + moving_piece_nnue_type * 64 + (to ^ 0b111000);
        changes.rem_white_feat(removed_white_index);
        changes.add_white_feat(add_white_index);
        changes.rem_black_feat(removed_black_index);
        changes.add_black_feat(add_black_index);
    }
    else if (promoteTo != Board::EMPTY) {
        //remove pawn from, add piece to
        int promote_to_type = promoteTo - side;
        int promote_to_type_nnue = (promote_to_type >> 1) - 1;
        int removed_white_index = side_nnue * 64 * 6 + moving_piece_nnue_type * 64 + from;
        int add_white_index = side_nnue * 64 * 6 + promote_to_type_nnue * 64 + to;
        int removed_black_index = (1 - side_nnue) * 64 * 6 + moving_piece_nnue_type * 64 + (from ^ 0b111000);
        int add_black_index = (1 - side_nnue) * 64 * 6 + promote_to_type_nnue * 64 + (to ^ 0b111000);
        changes.rem_white_feat(removed_white_index);
        changes.add_white_feat(add_white_index);
        changes.rem_black_feat(removed_black_index);
        changes.add_black_feat(add_black_index);
    }
    else if (is_castle) {
        int* sq = Board::CASTLE_SQS[from][side];
        //Castle convention!
        //mv = from(move) == 0 ? "e1g1" : "e1c1";
        int rem_white_king = side_nnue * 64 * 6 + KING_NNUE * 64 + sq[0];
        int add_white_king = side_nnue * 64 * 6 + KING_NNUE * 64 + sq[1];
        int rem_white_rook = side_nnue * 64 * 6 + ROOK_NNUE * 64 + sq[2];
        int add_white_rook = side_nnue * 64 * 6 + ROOK_NNUE * 64 + sq[3];
        changes.rem_white_feat(rem_white_king);
        changes.add_white_feat(add_white_king);
        changes.rem_white_feat(rem_white_rook);
        changes.add_white_feat(add_white_rook);
        int rem_black_king = (1 - side_nnue) * 64 * 6 + KING_NNUE * 64 + (sq[0] ^ 0b111000);
        int add_black_king = (1 - side_nnue) * 64 * 6 + KING_NNUE * 64 + (sq[1] ^ 0b111000);
        int rem_black_rook = (1 - side_nnue) * 64 * 6 + ROOK_NNUE * 64 + (sq[2] ^ 0b111000);
        int add_black_rook = (1 - side_nnue) * 64 * 6 + ROOK_NNUE * 64 + (sq[3] ^ 0b111000);        
        changes.rem_black_feat(rem_black_king);
        changes.add_black_feat(add_black_king);
        changes.rem_black_feat(rem_black_rook);
        changes.add_black_feat(add_black_rook);
    }
    else if (is_ep) {
        //board[from] = EMPTY; //pawn from
        //board[to] = movingPiece; //pawn to
        //board[to + MoveGen::epCaptDiff[side]] = EMPTY; //capt pawn
        int jp_sq = to + MoveGen::epCaptDiff[side];
        int removed_white_index = side_nnue * 64 * 6 + PAWN_NNUE * 64 + from;
        int add_white_index = side_nnue * 64 * 6 + PAWN_NNUE * 64 + to;
        int rem_cap_white_index = (1 - side_nnue) * 64 * 6 + PAWN_NNUE * 64 + jp_sq;
        changes.rem_white_feat(removed_white_index);
        changes.add_white_feat(add_white_index);
        changes.rem_white_feat(rem_cap_white_index);       
        int rem_black_index = (1 - side_nnue) * 64 * 6 + PAWN_NNUE * 64 + (from ^ 0b111000);
        int add_black_index = (1 - side_nnue) * 64 * 6 + PAWN_NNUE * 64 + (to ^ 0b111000);
        int rem_cap_black_index = side_nnue * 64 * 6 + PAWN_NNUE * 64 + (jp_sq ^ 0b111000);        
        changes.rem_black_feat(rem_black_index);
        changes.add_black_feat(add_black_index);
        changes.rem_black_feat(rem_cap_black_index);
    }
    else {      
        int removed_white_index = side_nnue * 64 * 6 + moving_piece_nnue_type * 64 + from;
        int add_white_index = side_nnue * 64 * 6 + moving_piece_nnue_type * 64 + to;
        int removed_black_index = (1 - side_nnue) * 64 * 6 + moving_piece_nnue_type * 64 + (from ^ 0b111000);
        int add_black_index = (1 - side_nnue) * 64 * 6 + moving_piece_nnue_type * 64 + (to ^ 0b111000);
        changes.rem_white_feat(removed_white_index);
        changes.add_white_feat(add_white_index);
        changes.rem_black_feat(removed_black_index);
        changes.add_black_feat(add_black_index);
        assert(removed_white_index >= 0 && removed_white_index < INPUT_SIZE);
        assert(add_white_index >= 0 && add_white_index < INPUT_SIZE);
        assert(removed_black_index >= 0 && removed_black_index < INPUT_SIZE);
        assert(add_black_index >= 0 && add_black_index < INPUT_SIZE);
    }

    if (capt != Board::EMPTY) {
        //rem to
        int capt_type = capt - opponent_side;     
        int capt_type_nnue = (capt_type >> 1) - 1;
        int removed_white_index = (1 - side_nnue) * 64 * 6 + capt_type_nnue * 64 + to;
        int removed_black_index = side_nnue * 64 * 6 + capt_type_nnue * 64 + (to ^ 0b111000);
        changes.rem_white_feat(removed_white_index);
        changes.rem_black_feat(removed_black_index);
        assert(removed_white_index >= 0 && removed_white_index < INPUT_SIZE);
        assert(removed_black_index >= 0 && removed_black_index < INPUT_SIZE);
    }
    //changes.print();
    return changes;
}
