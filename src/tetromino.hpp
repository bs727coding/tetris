#pragma once
// Tetromino shapes and SRS (Super Rotation System) data.
#include <array>
#include <cstdint>
#include <span>

inline constexpr int kBoardW   = 10;
inline constexpr int kBoardH   = 40;                 // 20 visible rows + 20 hidden rows above
inline constexpr int kVisibleH = 20;
inline constexpr int kHiddenH  = kBoardH - kVisibleH; // index of the top visible row

enum class PieceType : std::uint8_t { I, O, T, S, Z, J, L };
inline constexpr int kPieceTypes = 7;

struct Point { int x, y; };  // board coordinates, y grows downward
using Cells = std::array<Point, 4>;

// Cell offsets inside the piece's bounding box. rot: 0 = spawn, 1 = R, 2 = 180, 3 = L.
const Cells& shapeCells(PieceType type, int rot);

// Offsets to try (in order) when rotating from -> to. The first test is always {0, 0}.
std::span<const Point> kickTests(PieceType type, int from, int to);

char pieceLetter(PieceType type);
