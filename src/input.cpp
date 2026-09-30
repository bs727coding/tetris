#include "input.hpp"

#include "config.hpp"
#include "raylib.h"

#include <cmath>
#include <initializer_list>

namespace {

bool anyPressed(std::initializer_list<int> keys)
{
    for (int k : keys)
        if (IsKeyPressed(k))
            return true;
    return false;
}

// Auto-shifts that have fired after holding a direction for t seconds
int autoShifts(double t)
{
    if (t < cfg::kDas)
        return 0;
    return 1 + int(std::floor((t - cfg::kDas) / cfg::kArr));
}

} // namespace

InputFrame InputHandler::poll(double dt)
{
    InputFrame f;

    const bool left = IsKeyDown(KEY_LEFT);
    const bool right = IsKeyDown(KEY_RIGHT);
    if (IsKeyPressed(KEY_LEFT))  lastPressed_ = -1;
    if (IsKeyPressed(KEY_RIGHT)) lastPressed_ = 1;
    const int want = (left && right) ? lastPressed_ : left ? -1 : right ? 1 : 0;

    if (want != dir_) {
        dir_ = want;
        held_ = 0;
        f.shift = want;  // a tap moves one cell immediately
    } else if (dir_ != 0) {
        const double before = held_;
        held_ += dt;  // DAS keeps charging across piece spawns
        if (held_ >= cfg::kDas) {
            if constexpr (cfg::kArr <= 0)
                f.shift = dir_ * kBoardW;
            else
                f.shift = dir_ * (autoShifts(held_) - autoShifts(before));
        }
    }

    f.softDrop  = IsKeyDown(KEY_DOWN);
    f.hardDrop  = IsKeyPressed(KEY_SPACE);
    f.rotateCW  = anyPressed({ KEY_UP, KEY_X });
    f.rotateCCW = anyPressed({ KEY_Z, KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL });
    f.rotate180 = IsKeyPressed(KEY_A);
    f.hold      = anyPressed({ KEY_C, KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT });
    return f;
}

void InputHandler::reset()
{
    dir_ = 0;
    lastPressed_ = 0;
    held_ = 0;
}
