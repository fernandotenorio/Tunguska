#ifndef MOVE_PICKER_H
#define MOVE_PICKER_H

#include "Engine/Board.h"
#include "Engine/MoveList.h"
#include "Engine/MoveGen.h"

class MovePicker {
public:
    MovePicker(Board& b, int ttMove, bool inCheck);

    int next(); // returns Move::NO_MOVE when done

private:
    enum Stage {
        STAGE_TT,
        STAGE_CAPTURES_INIT,
        STAGE_GOOD_CAPTURES,
        STAGE_KILLER1,
        STAGE_KILLER2,
        STAGE_QUIETS_INIT,
        STAGE_QUIETS,
        STAGE_BAD_CAPTURES,
        STAGE_DONE
    };

    Stage stage;

    Board& board;
    int side;
    int ttMove;
    bool inCheck;

    MoveList captures;
    MoveList quiets;
    MoveList badCaptures;

    int captureScores[256];
    int quietScores[256];

    int capIndex;
    int quietIndex;

    void scoreCaptures();
    void scoreQuiets();
    int pickBestCapture();
    int pickBestQuiet();
};

#endif