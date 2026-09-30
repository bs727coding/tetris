#include "game.hpp"

#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

constexpr int kSpawnX = 3;
constexpr int kSpawnY = kHiddenH - 2;  // bounding box starts two rows above the field

bool rowFull(const BoardRow& row)
{
    return std::all_of(row.begin(), row.end(), [](std::uint8_t c) { return c != 0; });
}

} // namespace

Cells cellsOf(const ActivePiece& p)
{
    Cells out = shapeCells(p.type, p.rot);
    for (Point& c : out) {
        c.x += p.x;
        c.y += p.y;
    }
    return out;
}

bool collidesOn(const Board& board, const ActivePiece& p)
{
    for (const Point c : cellsOf(p)) {
        if (c.x < 0 || c.x >= kBoardW || c.y >= kBoardH)
            return true;
        if (c.y >= 0 && board[std::size_t(c.y)][std::size_t(c.x)] != 0)
            return true;
    }
    return false;
}

Game::Game(GameMode mode, int startLevel, std::uint64_t seed)
    : mode_(mode), rng_(seed)
{
    switch (mode) {
        case GameMode::Marathon:
        case GameMode::Ultra:  startLevel_ = std::clamp(startLevel, 1, cfg::kMaxStartLevel); break;
        case GameMode::Sprint: startLevel_ = 1; break;
        case GameMode::Zen:    startLevel_ = cfg::kZenLevel; break;
    }
    stats_.level = startLevel_;
    refillQueue();
    spawn(popQueue());
}

double Game::gravityInterval(int level)
{
    level = std::clamp(level, 1, cfg::kMaxLevel);
    return std::pow(0.8 - (level - 1) * 0.007, level - 1);
}

void Game::refillQueue()
{
    while (queue_.size() < std::size_t(cfg::kPreviewCount + kPieceTypes)) {
        std::array<PieceType, kPieceTypes> bag{ PieceType::I, PieceType::O, PieceType::T, PieceType::S,
                                                PieceType::Z, PieceType::J, PieceType::L };
        std::shuffle(bag.begin(), bag.end(), rng_);
        queue_.insert(queue_.end(), bag.begin(), bag.end());
    }
}

PieceType Game::popQueue()
{
    const PieceType t = queue_.front();
    queue_.pop_front();
    refillQueue();
    return t;
}

bool Game::spawn(PieceType type)
{
    piece_ = { type, kSpawnX, kSpawnY, 0 };
    fallAcc_ = 0;
    lockTimer_ = 0;
    lockResets_ = 0;
    lastWasRotation_ = false;
    lastKick_ = -1;

    if (collides(piece_)) {
        if (mode_ != GameMode::Zen) {  // block out
            finish(true);
            return false;
        }
        zenReset();
    }
    ActivePiece down = piece_;  // guideline: drop one row right away if possible
    ++down.y;
    if (!collides(down))
        piece_ = down;
    lowestY_ = piece_.y;
    ++spawnCount_;

    GameEvent e{ EventType::Spawn };
    e.piece = type;
    e.cells = cellsOf(piece_);
    emit(e);
    return true;
}

void Game::update(double dt, const InputFrame& in)
{
    if (over_)
        return;

    stats_.time += dt;
    if (mode_ == GameMode::Ultra && stats_.time >= cfg::kUltraSeconds) {
        stats_.time = cfg::kUltraSeconds;
        finish(false);
        return;
    }

    if (in.hold) {
        hold();
        if (over_)
            return;
    }
    if (in.rotateCW)  rotate(1);
    if (in.rotateCCW) rotate(-1);
    if (in.rotate180) rotate(2);
    if (in.shift != 0) shift(in.shift);

    if (in.hardDrop) {
        hardDrop();
        return;
    }

    // Gravity (and soft drop, which is just faster gravity)
    double interval = gravityInterval(stats_.level);
    if (in.softDrop)
        interval /= cfg::kSoftDropFactor;
    lastInterval_ = interval;

    if (!isGrounded()) {
        fallAcc_ += dt;
        int softRows = 0;
        for (int guard = 0; fallAcc_ >= interval && guard < kBoardH; ++guard) {
            fallAcc_ -= interval;
            if (!tryMove(0, 1))
                break;
            if (in.softDrop)
                ++softRows;
        }
        if (softRows > 0) {
            stats_.score += softRows;
            GameEvent e{ EventType::SoftDrop };
            e.piece = piece_.type;
            e.value = softRows;
            emit(e);
        }
    }

    if (isGrounded()) {
        fallAcc_ = 0;
        lockTimer_ += dt;
        if (lockTimer_ >= cfg::kLockDelay || lockResets_ >= cfg::kMaxLockResets)
            lockPiece();
    }
}

bool Game::isGrounded() const
{
    ActivePiece p = piece_;
    ++p.y;
    return collides(p);
}

bool Game::tryMove(int dx, int dy)
{
    ActivePiece p = piece_;
    p.x += dx;
    p.y += dy;
    if (collides(p))
        return false;
    piece_ = p;
    lastWasRotation_ = false;
    if (dy > 0) {
        lockTimer_ = 0;
        if (piece_.y > lowestY_) {  // a new lowest row earns a fresh set of lock resets
            lowestY_ = piece_.y;
            lockResets_ = 0;
        }
    }
    return true;
}

void Game::noteManipulation(bool wasGrounded)
{
    // Move reset: sliding or rotating on the ground refreshes the lock delay, up to a limit
    if (wasGrounded || isGrounded()) {
        if (lockResets_ < cfg::kMaxLockResets)
            lockTimer_ = 0;
        ++lockResets_;
    }
}

void Game::shift(int cells)
{
    const bool wasGrounded = isGrounded();
    const int dir = cells > 0 ? 1 : -1;
    int moved = 0;
    for (int i = 0; i < std::abs(cells); ++i) {
        if (!tryMove(dir, 0))
            break;
        ++moved;
    }
    if (moved == 0)
        return;
    noteManipulation(wasGrounded);
    GameEvent e{ EventType::Move };
    e.piece = piece_.type;
    e.value = moved * dir;
    emit(e);
}

void Game::rotate(int dir)
{
    if (piece_.type == PieceType::O)
        return;
    const bool wasGrounded = isGrounded();
    const int from = piece_.rot;
    const int to = (from + dir + 4) & 3;
    const auto tests = kickTests(piece_.type, from, to);
    for (int i = 0; i < int(tests.size()); ++i) {
        ActivePiece p = piece_;
        p.rot = to;
        p.x += tests[std::size_t(i)].x;
        p.y += tests[std::size_t(i)].y;
        if (collides(p))
            continue;
        piece_ = p;
        lastWasRotation_ = true;
        lastKick_ = (dir == 2) ? -1 : i;  // the "5th kick" T-spin rule only applies to 90-degree turns
        if (piece_.y > lowestY_) {
            lowestY_ = piece_.y;
            lockResets_ = 0;
            lockTimer_ = 0;
        }
        noteManipulation(wasGrounded);
        GameEvent e{ EventType::Rotate };
        e.piece = piece_.type;
        e.value = i;
        emit(e);
        return;
    }
}

void Game::hold()
{
    if (!canHold_)
        return;
    const PieceType current = piece_.type;
    const std::optional<PieceType> previous = hold_;
    hold_ = current;
    canHold_ = false;
    GameEvent e{ EventType::Hold };
    e.piece = current;
    emit(e);
    spawn(previous ? *previous : popQueue());
}

int Game::ghostY() const
{
    ActivePiece p = piece_;
    while (!collides(p))
        ++p.y;
    return p.y - 1;
}

void Game::hardDrop()
{
    const int fromY = piece_.y;
    const int distance = ghostY() - piece_.y;
    if (distance > 0) {
        piece_.y += distance;
        lastWasRotation_ = false;
    }
    stats_.score += 2 * distance;
    GameEvent e{ EventType::HardDrop };
    e.piece = piece_.type;
    e.cells = cellsOf(piece_);
    e.fromY = fromY;
    e.value = distance;
    emit(e);
    lockPiece();
}

Spin Game::detectSpin() const
{
    if (piece_.type != PieceType::T || !lastWasRotation_)
        return Spin::None;
    // 3-corner rule around the T's center; walls and floor count as filled
    const int cx = piece_.x + 1, cy = piece_.y + 1;
    auto filled = [&](int x, int y) {
        return x < 0 || x >= kBoardW || y >= kBoardH || (y >= 0 && board_[std::size_t(y)][std::size_t(x)] != 0);
    };
    const bool tl = filled(cx - 1, cy - 1), tr = filled(cx + 1, cy - 1);
    const bool bl = filled(cx - 1, cy + 1), br = filled(cx + 1, cy + 1);
    if (int(tl) + int(tr) + int(bl) + int(br) < 3)
        return Spin::None;
    bool frontA, frontB;  // the two corners the T is pointing at
    switch (piece_.rot) {
        case 0:  frontA = tl; frontB = tr; break;
        case 1:  frontA = tr; frontB = br; break;
        case 2:  frontA = bl; frontB = br; break;
        default: frontA = tl; frontB = bl; break;
    }
    if ((frontA && frontB) || lastKick_ == 4)
        return Spin::Full;
    return Spin::Mini;
}

void Game::lockPiece()
{
    const Cells cells = cellsOf(piece_);
    const Spin spin = detectSpin();
    bool allAboveField = true;
    int minY = kBoardH, maxY = -1;
    for (const Point c : cells) {
        if (c.y >= 0)
            board_[std::size_t(c.y)][std::size_t(c.x)] = std::uint8_t(int(piece_.type) + 1);
        allAboveField = allAboveField && c.y < kHiddenH;
        minY = std::min(minY, c.y);
        maxY = std::max(maxY, c.y);
    }
    ++stats_.pieces;
    canHold_ = true;

    GameEvent lockEv{ EventType::Lock };
    lockEv.piece = piece_.type;
    lockEv.cells = cells;
    emit(lockEv);

    // Collect and remove full rows
    GameEvent clear{ EventType::LineClear };
    clear.piece = piece_.type;
    clear.cells = cells;
    int lines = 0;
    for (int y = std::max(minY, 0); y <= maxY; ++y) {
        if (rowFull(board_[std::size_t(y)])) {
            clear.rows[std::size_t(lines)] = y;
            clear.rowCells[std::size_t(lines)] = board_[std::size_t(y)];
            ++lines;
        }
    }
    if (lines > 0) {
        Board collapsed{};
        int dst = kBoardH - 1;
        for (int y = kBoardH - 1; y >= 0; --y) {
            const bool cleared = std::find(clear.rows.begin(), clear.rows.begin() + lines, y) != clear.rows.begin() + lines;
            if (!cleared)
                collapsed[std::size_t(dst--)] = board_[std::size_t(y)];
        }
        board_ = collapsed;
    }

    const int levelBefore = stats_.level;
    scoreClear(lines, spin, clear);
    if (lines > 0 || spin != Spin::None) {
        clear.type = lines > 0 ? EventType::LineClear : EventType::SpinNoLines;
        emit(clear);
    }
    if (stats_.level > levelBefore) {
        GameEvent e{ EventType::LevelUp };
        e.value = stats_.level;
        emit(e);
    }

    if (allAboveField && lines == 0) {  // lock out
        if (mode_ != GameMode::Zen) {
            finish(true);
            return;
        }
        zenReset();
    }
    if (mode_ == GameMode::Sprint && stats_.lines >= cfg::kSprintLines) {
        finish(false);
        return;
    }
    spawn(popQueue());
}

void Game::scoreClear(int lines, Spin spin, GameEvent& ev)
{
    static constexpr int kLineScore[5]  = { 0, 100, 300, 500, 800 };
    static constexpr int kSpinScore[5]  = { 400, 800, 1200, 1600, 1600 };
    static constexpr int kMiniScore[5]  = { 100, 200, 400, 400, 400 };
    static constexpr int kPerfectBonus[5] = { 0, 800, 1200, 1800, 2000 };

    const int level = stats_.level;
    int base = spin == Spin::Full ? kSpinScore[lines] : spin == Spin::Mini ? kMiniScore[lines] : kLineScore[lines];
    const bool difficult = lines == 4 || (spin != Spin::None && lines > 0);

    bool b2b = false;
    if (lines > 0) {
        if (difficult) {
            if (b2bActive_) {
                base = base * 3 / 2;
                b2b = true;
                stats_.maxB2B = std::max(stats_.maxB2B, ++stats_.b2bStreak);
            }
            b2bActive_ = true;
        } else {
            b2bActive_ = false;
            stats_.b2bStreak = 0;
        }
        ++combo_;
        stats_.maxCombo = std::max(stats_.maxCombo, combo_);
    } else {
        combo_ = -1;
    }

    std::int64_t points = std::int64_t(base) * level;
    if (lines > 0 && combo_ > 0)
        points += 50LL * combo_ * level;

    bool perfect = false;
    if (lines > 0) {
        perfect = std::all_of(board_.begin(), board_.end(), [](const BoardRow& r) {
            return std::all_of(r.begin(), r.end(), [](std::uint8_t c) { return c == 0; });
        });
        if (perfect) {
            points += std::int64_t(lines == 4 && b2b ? 3200 : kPerfectBonus[lines]) * level;
            ++stats_.perfectClears;
        }
    }

    stats_.score += points;
    stats_.lines += lines;
    switch (lines) {
        case 1: ++stats_.singles; break;
        case 2: ++stats_.doubles; break;
        case 3: ++stats_.triples; break;
        case 4: ++stats_.tetrises; break;
        default: break;
    }
    if (spin != Spin::None)
        ++stats_.tspins;

    if (mode_ == GameMode::Marathon || mode_ == GameMode::Ultra)
        stats_.level = std::min(cfg::kMaxLevel, startLevel_ + stats_.lines / cfg::kLinesPerLevel);

    ev.lines = lines;
    ev.spin = spin;
    ev.b2b = b2b;
    ev.combo = lines > 0 ? combo_ : -1;
    ev.perfectClear = perfect;
    ev.points = points;
}

void Game::finish(bool toppedOut)
{
    over_ = true;
    toppedOut_ = toppedOut;
    GameEvent e{ toppedOut ? EventType::TopOut : EventType::Finished };
    e.piece = piece_.type;
    emit(e);
}

void Game::zenReset()
{
    for (BoardRow& row : board_)
        row.fill(0);
    b2bActive_ = false;
    combo_ = -1;
    emit(GameEvent{ EventType::ZenReset });
}

void Game::endSession()
{
    if (!over_)
        finish(false);
}

void Game::forceTopOut()
{
    if (!over_)
        finish(true);
}

void Game::setActive(PieceType type, int x, int y, int rot)
{
    piece_ = { type, x, y, rot };
    fallAcc_ = 0;
    lockTimer_ = 0;
    lockResets_ = 0;
    lowestY_ = y;
    lastWasRotation_ = false;
    lastKick_ = -1;
}

double Game::lockProgress() const
{
    return isGrounded() ? std::clamp(lockTimer_ / cfg::kLockDelay, 0.0, 1.0) : 0.0;
}

double Game::fallProgress() const
{
    if (over_ || isGrounded())
        return 0.0;
    return std::clamp(fallAcc_ / lastInterval_, 0.0, 1.0);
}

double Game::timeLeft() const
{
    return std::max(0.0, cfg::kUltraSeconds - stats_.time);
}

int Game::linesLeft() const
{
    return std::max(0, cfg::kSprintLines - stats_.lines);
}

int Game::stackHeight() const
{
    for (int y = 0; y < kBoardH; ++y)
        if (!std::all_of(board_[std::size_t(y)].begin(), board_[std::size_t(y)].end(), [](std::uint8_t c) { return c == 0; }))
            return kBoardH - y;
    return 0;
}
