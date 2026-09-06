#include "Engine/Board.h"
#include "Engine/Evaluation.h"
#include "Engine/Search.h"
#include "Engine/MoveGen.h"
#include "Engine/Magic.h"
#include "Engine/Zobrist.h"
#include "../engine/state.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static void material(const Board& b) {
    int expected[2] = {0, 0};
    for (int p : b.board) if (p) expected[p & 1] += abs(Evaluation::PIECE_VALUES[p]);
    require(expected[0] == b.material[0] && expected[1] == b.material[1], "material mismatch");
}

static unsigned long long walk(Board& b, int depth) {
    material(b);
    if (!depth) return 1;
    const auto fen = b.toFEN();
    const auto key = b.zKey;
    MoveList moves;
    MoveGen::pseudoLegalMoves(&b, b.state.currentPlayer, moves, false);
    unsigned long long count = 0;
    for (int i = 0; i < moves.size(); ++i) {
        const int move = moves.get(i);
        auto undo = b.makeMove(move);
        if (!undo.valid) continue;
        count += walk(b, depth - 1);
        b.undoMove(move, undo);
        require(b.zKey == key && b.toFEN() == fen, "make/undo position mismatch");
        material(b);
    }
    return count;
}

int main() {
    try {
        BitBoardGen::initAll(); Zobrist::init_keys(); Evaluation::initAll();
        Magic::magicArraysInit(); Search::init_search();
        auto board = Board::fromStartPosition();
        require(walk(board, 3) == 8902, "startpos perft mismatch");
        board.applyMoves("g1f3 g8f6 f3g1 f6g8");
        require(board.isRepetition(), "test repetition setup failed");
        const int gameHistory = board.histPly;
        const int oldLMR = Search::LMRTable[8][16];
        Tune::LMRBase = 175;
        Search::init_search();
        require(Search::LMRTable[8][16] == oldLMR + 1, "LMR table not rebuilt");
        Tune::PawnValue = 150; Tune::KnightValue = 500; Tune::BishopValue = 550;
        Tune::RookValue = 800; Tune::QueenValue = 1600;
        Tune::refresh(board);
        require(board.histPly == gameHistory && board.isRepetition(), "tuning erased game history");
        material(board);
        require(Evaluation::PAWN_VAL == 150 && Evaluation::PIECE_VALUES[3] == -150, "piece values stale");
        require(Search::MVV_LVA[2][4] == 155, "MVV-LVA table stale");
        auto changed = Board::fromStartPosition();
        require(walk(changed, 3) == 8902, "piece tuning changed move generation");
        for (const char* fen : {
            "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1",
            "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
            "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"}) {
            auto special = Board::fromFEN(fen);
            walk(special, 3);
        }
        std::cout << "Engine parameter, material, table and perft checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
