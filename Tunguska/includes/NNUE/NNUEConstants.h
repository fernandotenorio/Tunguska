#pragma once

#define INPUT_SIZE 768
#define HL_SIZE 1024
#define QA 255

// Enums
enum Side { BLACK_NNUE = 0, WHITE_NNUE = 1 };
enum PieceType { PAWN_NNUE = 0, KNIGHT_NNUE = 1, BISHOP_NNUE = 2, ROOK_NNUE = 3, QUEEN_NNUE = 4, KING_NNUE = 5 };
typedef int Square; // Use int for Square (0-63)

// Helper to convert Engine Piece (2..13) to NNUE Piece (0..11)
// White Pawn(2)->0 ... Black King(13)->11
constexpr int to_nnue_piece(int piece) {
    // Engine mapping: W_PAWN=2, B_PAWN=3...
    // We want: W_PAWN=0, W_N=1... B_P=6...
    // Simple lookup based on your Board.h constants:
    switch (piece) {
        case 2: return 0; // W_P
        case 4: return 1; // W_N
        case 6: return 2; // W_B
        case 8: return 3; // W_R
        case 10: return 4; // W_Q
        case 12: return 5; // W_K
        case 3: return 6; // B_P
        case 5: return 7; // B_N
        case 7: return 8; // B_B
        case 9: return 9; // B_R
        case 11: return 10; // B_Q
        case 13: return 11; // B_K
        default: return 0;
    }
}