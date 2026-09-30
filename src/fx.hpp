#pragma once
// Visual effects: particles, popups, flashes, trails, rings, screen shake and
// the springy row-collapse animation. All positions are in virtual coordinates.
#include "render.hpp"
#include "tetromino.hpp"

#include <array>
#include <string>
#include <vector>

enum class ParticleKind : unsigned char { Spark, Chunk, Confetti, Dust };

// A popup placed in a slot replaces whatever was showing in that slot
enum class Slot : int { None = -1, ClearName, B2B, Points, Combo, Banner, Center };

class Fx {
public:
    void update(float dt);
    void reset();

    void burst(Vector2 at, Color color, int count, float speed, ParticleKind kind, float life = 0.8f,
               float size = 6.0f, float upBias = 0.0f);
    void confetti(Vector2 at, int count, float speed, float angleDeg, float spreadDeg);
    void popup(const std::string& text, Vector2 at, float size, Color color, float duration = 1.2f,
               float delay = 0.0f, Face face = Face::Bold, float rise = 40.0f, Slot slot = Slot::None);
    void rowFlash(Rectangle area, Color color);
    void trail(Rectangle area, Color color);
    void ring(Vector2 center, float radius, Color color, float duration = 0.6f);
    void cellFlash(Rectangle cell, Color color, float duration = 0.18f);
    void shake(float trauma);
    void flash(Color color, float strength);

    // Rows that just cleared: rows above them slide down with a little bounce
    void collapseRows(const std::array<int, 4>& rows, int count);
    float rowOffset(int boardRow) const { return rowOff_[std::size_t(boardRow)]; }
    void clearRowOffsets();

    Vector2 shakeOffset() const;
    void drawWorld(Renderer& r, Pass pass) const;   // particles, flashes, trails, rings
    void drawPopups(Renderer& r, Pass pass) const;
    void drawScreenFlash(Renderer& r) const;       // screen space, after bloom

private:
    struct Particle {
        Vector2 pos, vel;
        float life, maxLife, size, rot, spin, drag, gravity;
        Color color;
        ParticleKind kind;
    };
    struct Popup {
        std::string text;
        Vector2 pos;
        float t, duration, delay, size, rise;
        Color color;
        Face face;
        Slot slot;
    };
    struct Timed {
        Rectangle area;
        Color color;
        float t, duration;
    };
    struct Ring {
        Vector2 center;
        float radius, t, duration;
        Color color;
    };

    std::vector<Particle> particles_;
    std::vector<Popup> popups_;
    std::vector<Timed> rowFlashes_, trails_, cellFlashes_;
    std::vector<Ring> rings_;
    std::array<float, kBoardH> rowOff_{}, rowVel_{};
    float trauma_ = 0.0f;
    float time_ = 0.0f;
    Color flashColor_{ 255, 255, 255, 255 };
    float flash_ = 0.0f;
    unsigned seed_ = 12345u;
    float rand01();
};
