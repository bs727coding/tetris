#include "ai.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace {

struct Candidate {
    double score = -std::numeric_limits<double>::infinity();
    int rot = 0;
    int x = 0;
};

// Weights from the well-known "near perfect" Tetris bot by Yiyuan Lee.
double evaluate(const Board& b, int lines)
{
    int heights[kBoardW]{};
    int holes = 0;
    for (int x = 0; x < kBoardW; ++x) {
        bool seenBlock = false;
        for (int y = 0; y < kBoardH; ++y) {
            if (b[std::size_t(y)][std::size_t(x)] != 0) {
                if (!seenBlock)
                    heights[x] = kBoardH - y;
                seenBlock = true;
            } else if (seenBlock) {
                ++holes;
            }
        }
    }
    int aggregate = 0, bumpiness = 0;
    for (int x = 0; x < kBoardW; ++x) {
        aggregate += heights[x];
        if (x + 1 < kBoardW)
            bumpiness += std::abs(heights[x] - heights[x + 1]);
    }
    return -0.510066 * aggregate + 0.760666 * lines - 0.35663 * holes - 0.184483 * bumpiness;
}

Candidate bestFor(const Board& board, PieceType type)
{
    Candidate best;
    const int rotations = type == PieceType::O ? 1 : 4;
    for (int rot = 0; rot < rotations; ++rot) {
        for (int x = -2; x < kBoardW; ++x) {
            ActivePiece p{ type, x, kHiddenH - 2, rot };
            if (collidesOn(board, p))
                continue;
            while (true) {
                ActivePiece down = p;
                ++down.y;
                if (collidesOn(board, down))
                    break;
                p = down;
            }
            Board after = board;
            for (const Point c : cellsOf(p))
                if (c.y >= 0)
                    after[std::size_t(c.y)][std::size_t(c.x)] = std::uint8_t(int(type) + 1);
            int lines = 0;
            Board collapsed{};
            int dst = kBoardH - 1;
            for (int y = kBoardH - 1; y >= 0; --y) {
                const auto& row = after[std::size_t(y)];
                if (std::all_of(row.begin(), row.end(), [](std::uint8_t c) { return c != 0; }))
                    ++lines;
                else
                    collapsed[std::size_t(dst--)] = row;
            }
            const double s = evaluate(collapsed, lines);
            if (s > best.score)
                best = { s, rot, x };
        }
    }
    return best;
}

} // namespace

AiPlan aiPlan(const Game& game)
{
    const Candidate now = bestFor(game.board(), game.active().type);
    AiPlan plan{ now.rot, now.x, false };
    if (game.canHold()) {
        const PieceType alt = game.held() ? *game.held() : game.next(0);
        const Candidate viaHold = bestFor(game.board(), alt);
        if (viaHold.score > now.score + 0.5)
            plan = { viaHold.rot, viaHold.x, true };
    }
    return plan;
}

InputFrame AutoPlayer::step(const Game& game, double dt)
{
    InputFrame f;
    if (game.over())
        return f;
    if (game.spawnCount() != planFor_) {
        planFor_ = game.spawnCount();
        plan_ = aiPlan(game);
        stuck_ = 0;
    }
    budget_ = std::min(budget_ + dt * speed_, 3.0);
    if (budget_ < 1.0)
        return f;
    budget_ -= 1.0;

    const ActivePiece& p = game.active();
    if (plan_.hold && game.canHold()) {
        f.hold = true;  // the new piece gets a fresh plan next frame
        plan_.hold = false;
    } else if (p.rot != plan_.rot) {
        const int diff = (plan_.rot - p.rot + 4) & 3;
        f.rotateCW = diff == 1;
        f.rotateCCW = diff == 3;
        f.rotate180 = diff == 2;
        if (++stuck_ > 8)
            f.hardDrop = true;
    } else if (p.x != plan_.x && stuck_ <= 12) {
        f.shift = plan_.x > p.x ? 1 : -1;
        ++stuck_;
    } else {
        f.hardDrop = true;
    }
    return f;
}
