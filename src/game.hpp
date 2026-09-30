#pragma once
// Pure rules engine (guideline Tetris). No rendering, no audio, no raylib:
// the presentation layer reads the state and reacts to the emitted events.
#include "tetromino.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <vector>

enum class GameMode : std::uint8_t { Marathon, Sprint, Ultra, Zen };
inline constexpr int kGameModes = 4;

// 0 = empty, 1..7 = PieceType + 1, kGreyCell = greyed-out block
using BoardRow = std::array<std::uint8_t, kBoardW>;
using Board    = std::array<BoardRow, kBoardH>;
inline constexpr std::uint8_t kGreyCell = 8;

struct ActivePiece {
    PieceType type = PieceType::T;
    int x = 0, y = 0;  // bounding-box origin on the board
    int rot = 0;
};

Cells cellsOf(const ActivePiece& p);
bool  collidesOn(const Board& board, const ActivePiece& p);

enum class Spin : std::uint8_t { None, Mini, Full };

// One frame of player intent, produced by InputHandler (or the AI)
struct InputFrame {
    int  shift     = 0;  // cells to move this frame (+right / -left)
    bool softDrop  = false;
    bool hardDrop  = false;
    bool rotateCW  = false;
    bool rotateCCW = false;
    bool rotate180 = false;
    bool hold      = false;
};

enum class EventType : std::uint8_t {
    Spawn, Move, Rotate, Hold, SoftDrop, HardDrop, Lock,
    LineClear, SpinNoLines, LevelUp, TopOut, Finished, ZenReset,
};

struct GameEvent {
    EventType type{};
    PieceType piece{};
    Cells cells{};    // absolute cells of the piece involved
    int fromY = 0;    // HardDrop: piece y before the drop
    int value = 0;    // HardDrop: distance, Rotate: kick index, LevelUp: new level
    // LineClear / SpinNoLines details
    int lines = 0;
    Spin spin = Spin::None;
    bool b2b = false;       // back-to-back bonus applied
    int combo = -1;         // 0 = first clear of a chain
    bool perfectClear = false;
    std::int64_t points = 0;
    std::array<int, 4> rows{};          // cleared rows (board indices before collapse), ascending
    std::array<BoardRow, 4> rowCells{}; // what those rows contained
};

struct Stats {
    std::int64_t score = 0;
    int lines = 0;
    int level = 1;
    int pieces = 0;
    double time = 0;
    int singles = 0, doubles = 0, triples = 0, tetrises = 0;
    int tspins = 0;        // full and mini, with or without lines
    int maxCombo = 0;
    int b2bStreak = 0, maxB2B = 0;
    int perfectClears = 0;
    double pps() const { return time > 0 ? pieces / time : 0.0; }
};

class Game {
public:
    Game(GameMode mode, int startLevel, std::uint64_t seed);

    void update(double dt, const InputFrame& in);

    GameMode mode() const { return mode_; }
    const Board& board() const { return board_; }
    const ActivePiece& active() const { return piece_; }
    Cells activeCells() const { return cellsOf(piece_); }
    int ghostY() const;
    std::optional<PieceType> held() const { return hold_; }
    bool canHold() const { return canHold_; }
    PieceType next(int i) const { return queue_[std::size_t(i)]; }
    const Stats& stats() const { return stats_; }
    int startLevel() const { return startLevel_; }
    int spawnCount() const { return spawnCount_; }
    int combo() const { return combo_; }            // -1 = no active combo
    bool backToBack() const { return b2bActive_; }  // last clear was a tetris or T-spin

    bool over() const { return over_; }
    bool toppedOut() const { return over_ && toppedOut_; }
    bool completed() const { return over_ && !toppedOut_; }  // goal reached / time up / session ended

    double lockProgress() const;  // 0..1 while grounded
    double fallProgress() const;  // 0..1 progress toward the next gravity step
    double timeLeft() const;      // Ultra
    int linesLeft() const;        // Sprint
    int stackHeight() const;      // rows from the floor to the highest block
    bool isGrounded() const;

    std::vector<GameEvent>& events() { return events_; }

    void endSession();   // Zen: the player finishes the run
    void forceTopOut();  // used by --autotest

    // Test hooks
    Board& mutableBoard() { return board_; }
    void setActive(PieceType type, int x, int y, int rot);
    void debugSpawn(PieceType type) { spawn(type); }

    static double gravityInterval(int level);  // seconds per row

private:
    bool collides(const ActivePiece& p) const { return collidesOn(board_, p); }
    void refillQueue();
    PieceType popQueue();
    bool spawn(PieceType type);
    bool tryMove(int dx, int dy);
    void shift(int cells);
    void rotate(int dir);
    void hold();
    void hardDrop();
    void lockPiece();
    void noteManipulation(bool wasGrounded);
    Spin detectSpin() const;
    void scoreClear(int lines, Spin spin, GameEvent& ev);
    void finish(bool toppedOut);
    void zenReset();
    void emit(const GameEvent& e) { events_.push_back(e); }

    GameMode mode_;
    int startLevel_ = 1;
    Board board_{};
    ActivePiece piece_{};
    std::deque<PieceType> queue_;
    std::mt19937_64 rng_;
    std::optional<PieceType> hold_;
    bool canHold_ = true;
    Stats stats_{};

    double fallAcc_ = 0;
    double lastInterval_ = 1;
    double lockTimer_ = 0;
    int lockResets_ = 0;
    int lowestY_ = 0;
    bool lastWasRotation_ = false;
    int lastKick_ = -1;

    bool b2bActive_ = false;
    int combo_ = -1;
    int spawnCount_ = 0;
    bool over_ = false;
    bool toppedOut_ = false;
    std::vector<GameEvent> events_;
};
