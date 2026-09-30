#pragma once
// A small placement bot: plays the title-screen attract mode and drives --autotest.
#include "game.hpp"

struct AiPlan {
    int rot = 0;
    int x = 0;
    bool hold = false;
};

// Best placement for the current piece (optionally via hold), using a classic
// height / holes / bumpiness / lines heuristic.
AiPlan aiPlan(const Game& game);

// Feeds a Game with human-like inputs toward the plan, a few actions per second.
class AutoPlayer {
public:
    explicit AutoPlayer(double actionsPerSecond) : speed_(actionsPerSecond) {}
    InputFrame step(const Game& game, double dt);
    void setSpeed(double actionsPerSecond) { speed_ = actionsPerSecond; }

private:
    double speed_;
    double budget_ = 0;
    int planFor_ = -1;  // spawnCount the current plan belongs to
    int stuck_ = 0;
    AiPlan plan_{};
};
