#include "Engine/MovePicker.h"
#include "Engine/Move.h"
#include "Engine/Search.h"


MovePicker::MovePicker(Board& b, int ttm, bool check)
    : board(b),
      side(b.state.currentPlayer),
      ttMove(ttm),
      inCheck(check),
      stage(STAGE_TT),
      capIndex(0),
      quietIndex(0)
{}

void MovePicker::scoreCaptures() {
    for (int i = 0; i < captures.size(); i++) {

        int move = captures.get(i);
        int from = Move::from(move);
        int to   = Move::to(move);

        int attacker = board.board[from];
        int victim   = board.board[to];

        captureScores[i] = Search::MVV_LVA[victim][attacker];
    }
}

void MovePicker::scoreQuiets() {
    for (int i = 0; i < quiets.size(); i++) {

        int move = quiets.get(i);
        int from = Move::from(move);
        int to   = Move::to(move);

        int piece = board.board[from];

        quietScores[i] = board.searchHistory[piece][to];
    }
}

int MovePicker::pickBestCapture() {
    int best = capIndex;

    for (int i = capIndex + 1; i < captures.size(); i++)
        if (captureScores[i] > captureScores[best])
            best = i;

    std::swap(captureScores[capIndex], captureScores[best]);
    captures.swap(capIndex, best);

    int move = captures.get(capIndex++);
    int from = Move::from(move);
    int to = Move::to(move);
    int piece = board.board[from];
    int captured = Move::captured(move);
    int seeScore = see(&board, to, captured, from, piece);

    if (seeScore < 0) {
        badCaptures.add(move);
        return next();
    }
    return move;
}

int MovePicker::pickBestQuiet() {
    int best = quietIndex;

    for (int i = quietIndex + 1; i < quiets.size(); i++)
        if (quietScores[i] > quietScores[best])
            best = i;

    std::swap(quietScores[quietIndex], quietScores[best]);
    quiets.swap(quietIndex, best);

    return quiets.get(quietIndex++);
}

int MovePicker::next() {
    while (true) {
        switch (stage) {

        case STAGE_TT:
            stage = STAGE_CAPTURES_INIT;
            if (ttMove != Move::NO_MOVE)
                return ttMove;
            break;

        case STAGE_CAPTURES_INIT:
            captures.reset();
            badCaptures.reset();
            MoveGen::pseudoLegalCaptureMoves(&board, side, captures);
            scoreCaptures();
            capIndex = 0;
            stage = STAGE_GOOD_CAPTURES;
            break;

        case STAGE_GOOD_CAPTURES:
            if (capIndex < captures.size())
                return pickBestCapture();
            stage = STAGE_KILLER1;
            break;

        case STAGE_KILLER1: {
            stage = STAGE_KILLER2;
            int k = board.searchKillers[0][board.ply];
            if (k != Move::NO_MOVE && k != ttMove)
                return k;
            break;
        }

        case STAGE_KILLER2: {
            stage = STAGE_QUIETS_INIT;
            int k = board.searchKillers[1][board.ply];
            if (k != Move::NO_MOVE && k != ttMove)
                return k;
            break;
        }

        case STAGE_QUIETS_INIT:
            quiets.reset();
            MoveGen::pseudoLegalQuietMoves(&board, side, quiets);
            scoreQuiets();
            quietIndex = 0;
            stage = STAGE_QUIETS;
            break;

        case STAGE_QUIETS:
            if (quietIndex < quiets.size())
                return pickBestQuiet();
            stage = STAGE_BAD_CAPTURES;
            break;

        case STAGE_BAD_CAPTURES:
            if (!badCaptures.empty())
                return badCaptures.pop();
            stage = STAGE_DONE;
            break;

        case STAGE_DONE:
            return Move::NO_MOVE;
        }
    }
}