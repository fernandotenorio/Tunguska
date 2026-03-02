#pragma once

#define INPUT_SIZE 768
#define HL_SIZE 256
#define SCALE 400
#define QA 255
#define QB 64

// Enums
enum Side { BLACK_NNUE = 0, WHITE_NNUE = 1 };
enum PieceType { PAWN_NNUE = 0, KNIGHT_NNUE = 1, BISHOP_NNUE = 2, ROOK_NNUE = 3, QUEEN_NNUE = 4, KING_NNUE = 5 };
typedef int Square; // Use int for Square (0-63)