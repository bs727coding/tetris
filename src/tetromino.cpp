#include "tetromino.hpp"

namespace {

// Spawn orientation of every piece inside its bounding box (I uses 4x4, others 3x3).
constexpr Cells kSpawn[kPieceTypes] = {
    Cells{ { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 } } },  // I
    Cells{ { { 1, 0 }, { 2, 0 }, { 1, 1 }, { 2, 1 } } },  // O
    Cells{ { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } } },  // T
    Cells{ { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 } } },  // S
    Cells{ { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } } },  // Z
    Cells{ { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } } },  // J
    Cells{ { { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } } },  // L
};

// SRS rotation is a true rotation of the bounding box; O never changes.
struct ShapeTable {
    Cells cells[kPieceTypes][4]{};
    constexpr ShapeTable() {
        for (int t = 0; t < kPieceTypes; ++t) {
            cells[t][0] = kSpawn[t];
            const int n = (t == int(PieceType::I)) ? 4 : 3;
            for (int r = 1; r < 4; ++r)
                for (int i = 0; i < 4; ++i) {
                    const Point p = cells[t][r - 1][i];
                    cells[t][r][i] = (t == int(PieceType::O)) ? p : Point{ n - 1 - p.y, p.x };
                }
        }
    }
};
constexpr ShapeTable kShapes;

// Kick tables from the Tetris guideline, converted to y-down.
// Row order: 0->R, R->0, R->2, 2->R, 2->L, L->2, L->0, 0->L
constexpr Point kKicksJLSTZ[8][5] = {
    { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0,  2 }, { -1,  2 } },
    { { 0, 0 }, {  1, 0 }, {  1,  1 }, { 0, -2 }, {  1, -2 } },
    { { 0, 0 }, {  1, 0 }, {  1,  1 }, { 0, -2 }, {  1, -2 } },
    { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0,  2 }, { -1,  2 } },
    { { 0, 0 }, {  1, 0 }, {  1, -1 }, { 0,  2 }, {  1,  2 } },
    { { 0, 0 }, { -1, 0 }, { -1,  1 }, { 0, -2 }, { -1, -2 } },
    { { 0, 0 }, { -1, 0 }, { -1,  1 }, { 0, -2 }, { -1, -2 } },
    { { 0, 0 }, {  1, 0 }, {  1, -1 }, { 0,  2 }, {  1,  2 } },
};
constexpr Point kKicksI[8][5] = {
    { { 0, 0 }, { -2, 0 }, {  1, 0 }, { -2,  1 }, {  1, -2 } },
    { { 0, 0 }, {  2, 0 }, { -1, 0 }, {  2, -1 }, { -1,  2 } },
    { { 0, 0 }, { -1, 0 }, {  2, 0 }, { -1, -2 }, {  2,  1 } },
    { { 0, 0 }, {  1, 0 }, { -2, 0 }, {  1,  2 }, { -2, -1 } },
    { { 0, 0 }, {  2, 0 }, { -1, 0 }, {  2, -1 }, { -1,  2 } },
    { { 0, 0 }, { -2, 0 }, {  1, 0 }, { -2,  1 }, {  1, -2 } },
    { { 0, 0 }, {  1, 0 }, { -2, 0 }, {  1,  2 }, { -2, -1 } },
    { { 0, 0 }, { -1, 0 }, {  2, 0 }, { -1, -2 }, {  2,  1 } },
};
// 180-degree kicks (SRS+ style). Row order: 0->2, 2->0, R->L, L->R
constexpr Point kKicks180[4][6] = {
    { { 0, 0 }, {  0, -1 }, {  1, -1 }, { -1, -1 }, {  1, 0 }, { -1, 0 } },
    { { 0, 0 }, {  0,  1 }, { -1,  1 }, {  1,  1 }, { -1, 0 }, {  1, 0 } },
    { { 0, 0 }, {  1,  0 }, {  1, -2 }, {  1, -1 }, {  0, -2 }, { 0, -1 } },
    { { 0, 0 }, { -1,  0 }, { -1, -2 }, { -1, -1 }, {  0, -2 }, { 0, -1 } },
};
constexpr Point kNoKick[1] = { { 0, 0 } };

int kickRow90(int from, int to)
{
    switch (from * 4 + to) {
        case 0 * 4 + 1: return 0;
        case 1 * 4 + 0: return 1;
        case 1 * 4 + 2: return 2;
        case 2 * 4 + 1: return 3;
        case 2 * 4 + 3: return 4;
        case 3 * 4 + 2: return 5;
        case 3 * 4 + 0: return 6;
        default:        return 7;  // 0 -> L
    }
}

int kickRow180(int from)
{
    switch (from) {
        case 0:  return 0;
        case 2:  return 1;
        case 1:  return 2;
        default: return 3;
    }
}

} // namespace

const Cells& shapeCells(PieceType type, int rot)
{
    return kShapes.cells[int(type)][rot & 3];
}

std::span<const Point> kickTests(PieceType type, int from, int to)
{
    if (type == PieceType::O)
        return kNoKick;
    if (((from - to) & 3) == 2)
        return kKicks180[kickRow180(from)];
    const int row = kickRow90(from, to);
    return type == PieceType::I ? std::span<const Point>(kKicksI[row]) : std::span<const Point>(kKicksJLSTZ[row]);
}

char pieceLetter(PieceType type)
{
    return "IOTSZJL"[int(type)];
}
