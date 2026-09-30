#pragma once
// Keyboard -> InputFrame, with DAS/ARR auto-shift.
#include "game.hpp"

class InputHandler {
public:
    InputFrame poll(double dt);  // call once per frame while a game is running
    void reset();                // forget held keys (e.g. after pausing)

private:
    int dir_ = 0;          // direction currently auto-shifting (-1, 0, +1)
    int lastPressed_ = 0;  // most recent of left/right, wins when both are held
    double held_ = 0;      // how long dir_ has been held
};
