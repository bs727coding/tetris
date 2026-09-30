// Headless tests for the rules engine (links game.cpp + tetromino.cpp only).
// Run with `mingw32-make test`.
#include "config.hpp"
#include "game.hpp"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <set>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                \
    do {                                                                              \
        ++g_checks;                                                                   \
        const long long va_ = (long long)(a), vb_ = (long long)(b);                   \
        if (va_ != vb_) {                                                             \
            ++g_failures;                                                             \
            std::printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, \
                        #a, #b, va_, vb_);                                            \
        }                                                                             \
    } while (0)

// Fill the bottom of the board from text rows ('#' = block, '.' = empty), last row = floor.
void setBottom(Game& g, std::initializer_list<const char*> rows)
{
    Board& b = g.mutableBoard();
    for (auto& r : b)
        r.fill(0);
    int y = kBoardH - int(rows.size());
    for (const char* row : rows) {
        for (int x = 0; x < kBoardW; ++x)
            b[std::size_t(y)][std::size_t(x)] = row[x] == '#' ? kGreyCell : 0;
        ++y;
    }
}

void clearBoard(Game& g)
{
    for (auto& r : g.mutableBoard())
        r.fill(0);
}

const GameEvent* findEvent(Game& g, EventType type)
{
    for (const GameEvent& e : g.events())
        if (e.type == type)
            return &e;
    return nullptr;
}

InputFrame press(void (*set)(InputFrame&))
{
    InputFrame f;
    set(f);
    return f;
}

const InputFrame kIdle{};
const InputFrame kHardDrop = press([](InputFrame& f) { f.hardDrop = true; });
const InputFrame kRotCW = press([](InputFrame& f) { f.rotateCW = true; });
const InputFrame kRotCCW = press([](InputFrame& f) { f.rotateCCW = true; });
const InputFrame kHold = press([](InputFrame& f) { f.hold = true; });

InputFrame shiftBy(int n)
{
    InputFrame f;
    f.shift = n;
    return f;
}

void testBagIsFair()
{
    Game g(GameMode::Marathon, 1, 1234);
    std::string seq;
    for (int i = 0; i < 7 * 60; ++i) {
        seq += pieceLetter(g.active().type);
        clearBoard(g);
        g.update(0.0, kHardDrop);
        g.events().clear();
    }
    bool everyBagComplete = true;
    for (std::size_t bag = 0; bag < seq.size(); bag += 7) {
        std::set<char> s(seq.begin() + long(bag), seq.begin() + long(bag) + 7);
        everyBagComplete = everyBagComplete && s.size() == 7;
    }
    CHECK(everyBagComplete);
    CHECK(!g.over());
}

void testSpawnAndWalls()
{
    Game g(GameMode::Marathon, 1, 7);
    g.debugSpawn(PieceType::T);
    CHECK_EQ(g.active().x, 3);
    CHECK_EQ(g.active().y, kHiddenH - 1);  // spawned above the field, then dropped one row
    g.update(0.0, shiftBy(-20));
    CHECK_EQ(g.active().x, 0);
    g.update(0.0, shiftBy(20));
    CHECK_EQ(g.active().x, kBoardW - 3);
}

void testBasicRotationAndIKick()
{
    Game g(GameMode::Marathon, 1, 7);
    g.debugSpawn(PieceType::T);
    const ActivePiece before = g.active();
    g.update(0.0, kRotCW);
    CHECK_EQ(g.active().rot, 1);
    CHECK_EQ(g.active().x, before.x);
    CHECK_EQ(g.active().y, before.y);

    // Vertical I against the right wall: R->2 must kick one cell left (test 2)
    clearBoard(g);
    g.setActive(PieceType::I, 7, 30, 1);
    g.events().clear();
    g.update(0.0, kRotCW);
    CHECK_EQ(g.active().rot, 2);
    CHECK_EQ(g.active().x, 6);
    const GameEvent* rot = findEvent(g, EventType::Rotate);
    CHECK(rot != nullptr && rot->value == 1);
}

void testTSpinDouble()
{
    Game g(GameMode::Marathon, 1, 7);
    setBottom(g, {
        "...#......",
        "#...######",
        "##.#######",
    });
    g.setActive(PieceType::T, 1, 37, 3);  // pointing left, resting in the slot
    g.events().clear();
    g.update(0.0, kRotCCW);               // L -> 2 with no kick
    CHECK_EQ(g.active().rot, 2);
    g.update(0.0, kHardDrop);
    const GameEvent* e = findEvent(g, EventType::LineClear);
    CHECK(e != nullptr);
    if (e) {
        CHECK_EQ(e->lines, 2);
        CHECK(e->spin == Spin::Full);
        CHECK_EQ(e->points, 1200);
    }
    CHECK_EQ(g.stats().score, 1200);
}

void testTSpinTripleUsesFifthKick()
{
    Game g(GameMode::Marathon, 1, 7);
    setBottom(g, {
        "...#......",
        "..........",
        "###.######",
        "###..#####",
        "###.######",
    });
    g.setActive(PieceType::T, 3, 35, 0);
    g.events().clear();
    g.update(0.0, kRotCW);
    const GameEvent* rot = findEvent(g, EventType::Rotate);
    CHECK(rot != nullptr && rot->value == 4);
    CHECK_EQ(g.active().x, 2);
    CHECK_EQ(g.active().y, 37);
    g.update(0.0, kHardDrop);
    const GameEvent* e = findEvent(g, EventType::LineClear);
    CHECK(e != nullptr);
    if (e) {
        CHECK_EQ(e->lines, 3);
        CHECK(e->spin == Spin::Full);
        CHECK_EQ(e->points, 1600);
    }
}

void testTSpinMini()
{
    Game g(GameMode::Marathon, 1, 7);
    setBottom(g, {
        "...#......",
        "###...####",
        "####.#####",
    });
    g.setActive(PieceType::T, 3, 37, 1);
    g.events().clear();
    g.update(0.0, kRotCCW);  // R -> 0, no kick
    CHECK_EQ(g.active().rot, 0);
    g.update(0.0, kHardDrop);
    const GameEvent* e = findEvent(g, EventType::LineClear);
    CHECK(e != nullptr);
    if (e) {
        CHECK_EQ(e->lines, 1);
        CHECK(e->spin == Spin::Mini);
        CHECK_EQ(e->points, 200);
    }
}

// Drop a vertical I into the rightmost column
void dropIRight(Game& g)
{
    g.debugSpawn(PieceType::I);
    g.update(0.0, kRotCW);
    g.update(0.0, shiftBy(20));
    g.update(0.0, kHardDrop);
}

void testTetrisBackToBackAndCombo()
{
    Game g(GameMode::Marathon, 1, 7);
    setBottom(g, {
        "#########.", "#########.", "#########.", "#########.",
        "#########.", "#########.", "#########.", "#########.",
    });
    g.events().clear();
    dropIRight(g);
    const GameEvent* first = findEvent(g, EventType::LineClear);
    CHECK(first != nullptr);
    if (first) {
        CHECK_EQ(first->lines, 4);
        CHECK(!first->b2b);
        CHECK_EQ(first->combo, 0);
        CHECK_EQ(first->points, 800);
        CHECK(!first->perfectClear);
    }
    g.events().clear();
    dropIRight(g);
    const GameEvent* second = findEvent(g, EventType::LineClear);
    CHECK(second != nullptr);
    if (second) {
        CHECK(second->b2b);
        CHECK_EQ(second->combo, 1);
        CHECK(second->perfectClear);
        CHECK_EQ(second->points, 1200 + 50 + 3200);  // B2B tetris + combo + B2B tetris perfect clear
    }
    CHECK_EQ(g.stats().tetrises, 2);
    CHECK_EQ(g.stats().lines, 8);
}

void testPerfectClearSingleTetris()
{
    Game g(GameMode::Marathon, 1, 7);
    setBottom(g, { "#########.", "#########.", "#########.", "#########." });
    g.events().clear();
    dropIRight(g);
    const GameEvent* e = findEvent(g, EventType::LineClear);
    CHECK(e != nullptr);
    if (e) {
        CHECK(e->perfectClear);
        CHECK_EQ(e->points, 800 + 2000);
    }
}

void testLockDelayAndResetCap()
{
    Game g(GameMode::Marathon, 1, 7);
    clearBoard(g);
    g.setActive(PieceType::T, 3, kBoardH - 2, 0);  // resting on the floor
    for (int i = 0; i < 4; ++i)
        g.update(0.1, kIdle);
    CHECK_EQ(g.stats().pieces, 0);  // 0.4 s < lock delay
    g.update(0.11, kIdle);
    CHECK_EQ(g.stats().pieces, 1);

    // Wiggling on the ground refreshes the lock delay, but only kMaxLockResets times
    clearBoard(g);
    g.setActive(PieceType::T, 3, kBoardH - 2, 0);
    int lockedAt = -1;
    for (int i = 0; i < 40 && lockedAt < 0; ++i) {
        g.update(0.1, shiftBy(i % 2 == 0 ? 1 : -1));
        if (g.stats().pieces == 2)
            lockedAt = i;
    }
    CHECK_EQ(lockedAt, cfg::kMaxLockResets - 1);
}

void testHold()
{
    Game g(GameMode::Marathon, 1, 7);
    const PieceType first = g.active().type;
    const PieceType second = g.next(0);
    g.update(0.0, kHold);
    CHECK(g.held().has_value() && *g.held() == first);
    CHECK(g.active().type == second);
    CHECK(!g.canHold());
    g.update(0.0, kHold);  // ignored until the next piece locks
    CHECK(g.active().type == second);
    g.update(0.0, kHardDrop);
    CHECK(g.canHold());
}

void testTopOutAndZen()
{
    auto blockSpawn = [](Game& g) {
        setBottom(g, {});
        Board& b = g.mutableBoard();
        for (int y = kHiddenH + 1; y < kBoardH; ++y)
            for (int x = 0; x < kBoardW - 1; ++x)
                b[std::size_t(y)][std::size_t(x)] = kGreyCell;
        g.debugSpawn(PieceType::T);
        g.update(0.0, kHardDrop);  // locks at the top; the next spawn is blocked
    };

    Game marathon(GameMode::Marathon, 1, 7);
    blockSpawn(marathon);
    CHECK(marathon.over());
    CHECK(marathon.toppedOut());
    CHECK(findEvent(marathon, EventType::TopOut) != nullptr);

    Game zen(GameMode::Zen, 1, 7);
    blockSpawn(zen);
    CHECK(!zen.over());
    CHECK(findEvent(zen, EventType::ZenReset) != nullptr);
    zen.endSession();
    CHECK(zen.completed());
}

void testSprintAndUltraEndings()
{
    Game sprint(GameMode::Sprint, 1, 7);
    for (int i = 0; i < 10 && !sprint.over(); ++i) {
        setBottom(sprint, { "#########.", "#########.", "#########.", "#########." });
        dropIRight(sprint);
    }
    CHECK_EQ(sprint.stats().lines, 40);
    CHECK(sprint.completed());
    CHECK(findEvent(sprint, EventType::Finished) != nullptr);

    Game ultra(GameMode::Ultra, 1, 7);
    for (int i = 0; i < 130 && !ultra.over(); ++i) {
        clearBoard(ultra);  // keep the idle stack from topping out first
        ultra.update(1.0, kIdle);
    }
    CHECK(ultra.completed());
    CHECK(ultra.stats().time == cfg::kUltraSeconds);
}

void testGravityAndLevels()
{
    CHECK(Game::gravityInterval(1) == 1.0);
    bool decreasing = true;
    for (int l = 2; l <= cfg::kMaxLevel; ++l)
        decreasing = decreasing && Game::gravityInterval(l) < Game::gravityInterval(l - 1);
    CHECK(decreasing);

    Game g(GameMode::Marathon, 3, 7);
    CHECK_EQ(g.stats().level, 3);
    for (int i = 0; i < 3; ++i) {  // 12 lines -> level 4
        setBottom(g, { "#########.", "#########.", "#########.", "#########." });
        dropIRight(g);
    }
    CHECK_EQ(g.stats().level, 4);
    CHECK(findEvent(g, EventType::LevelUp) != nullptr);
}

} // namespace

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "7-bag randomizer is fair", testBagIsFair },
        { "spawn position and walls", testSpawnAndWalls },
        { "rotation and I-piece wall kick", testBasicRotationAndIKick },
        { "T-spin double", testTSpinDouble },
        { "T-spin triple via 5th kick", testTSpinTripleUsesFifthKick },
        { "T-spin mini", testTSpinMini },
        { "tetris, back-to-back, combo", testTetrisBackToBackAndCombo },
        { "perfect clear bonus", testPerfectClearSingleTetris },
        { "lock delay and reset cap", testLockDelayAndResetCap },
        { "hold", testHold },
        { "top out and Zen reset", testTopOutAndZen },
        { "Sprint and Ultra endings", testSprintAndUltraEndings },
        { "gravity curve and levels", testGravityAndLevels },
    };
    for (const Test& t : tests) {
        const int before = g_failures;
        t.fn();
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[FAIL]", t.name);
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
