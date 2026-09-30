#include "fx.hpp"

#include "config.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegToRad = 0.01745329f;
constexpr std::size_t kMaxParticles = 4000;

float easeOutBack(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

Color withAlpha(Color c, float a)
{
    c.a = (unsigned char)std::clamp(a * float(c.a), 0.0f, 255.0f);
    return c;
}

template <typename T, typename Pred>
void eraseIf(std::vector<T>& v, Pred pred)
{
    v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());
}

} // namespace

float Fx::rand01()
{
    seed_ = seed_ * 1664525u + 1013904223u;
    return float(seed_ >> 8) / 16777216.0f;
}

void Fx::reset()
{
    particles_.clear();
    popups_.clear();
    rowFlashes_.clear();
    trails_.clear();
    cellFlashes_.clear();
    rings_.clear();
    clearRowOffsets();
    trauma_ = 0.0f;
    flash_ = 0.0f;
}

void Fx::clearRowOffsets()
{
    rowOff_.fill(0.0f);
    rowVel_.fill(0.0f);
}

void Fx::update(float dt)
{
    time_ += dt;
    trauma_ = std::max(0.0f, trauma_ - dt * 1.6f);
    flash_ = std::max(0.0f, flash_ - dt * 2.2f);

    for (Particle& p : particles_) {
        p.life -= dt;
        p.vel.y += p.gravity * dt;
        const float drag = std::exp(-p.drag * dt);
        p.vel.x *= drag;
        p.vel.y *= drag;
        if (p.kind == ParticleKind::Confetti)  // flutter
            p.vel.x += std::sin(time_ * 7.0f + p.spin) * 60.0f * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.rot += p.spin * dt;
    }
    eraseIf(particles_, [](const Particle& p) { return p.life <= 0.0f; });

    for (Popup& p : popups_)
        p.t += dt;
    eraseIf(popups_, [](const Popup& p) { return p.t >= p.delay + p.duration; });

    for (auto* list : { &rowFlashes_, &trails_, &cellFlashes_ }) {
        for (Timed& f : *list)
            f.t += dt;
        eraseIf(*list, [](const Timed& f) { return f.t >= f.duration; });
    }
    for (Ring& r : rings_)
        r.t += dt;
    eraseIf(rings_, [](const Ring& r) { return r.t >= r.duration; });

    // Critically-ish damped springs pull collapsing rows into place
    for (std::size_t y = 0; y < rowOff_.size(); ++y) {
        if (rowOff_[y] == 0.0f && rowVel_[y] == 0.0f)
            continue;
        const float accel = -520.0f * rowOff_[y] - 34.0f * rowVel_[y];
        rowVel_[y] += accel * dt;
        rowOff_[y] += rowVel_[y] * dt;
        if (std::fabs(rowOff_[y]) < 0.002f && std::fabs(rowVel_[y]) < 0.02f)
            rowOff_[y] = rowVel_[y] = 0.0f;
    }
}

void Fx::burst(Vector2 at, Color color, int count, float speed, ParticleKind kind, float life, float size, float upBias)
{
    for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
        const float a = rand01() * 6.2831853f;
        const float s = speed * (0.35f + 0.65f * rand01());
        Particle p{};
        p.pos = at;
        p.vel = { std::cos(a) * s, std::sin(a) * s - upBias };
        p.maxLife = p.life = life * (0.6f + 0.4f * rand01());
        p.size = size * (0.6f + 0.6f * rand01());
        p.rot = rand01() * 360.0f;
        p.spin = (rand01() - 0.5f) * 720.0f;
        p.color = color;
        p.kind = kind;
        switch (kind) {
            case ParticleKind::Spark:    p.drag = 3.2f; p.gravity = 260.0f; break;
            case ParticleKind::Chunk:    p.drag = 1.2f; p.gravity = 900.0f; break;
            case ParticleKind::Confetti: p.drag = 2.0f; p.gravity = 220.0f; break;
            case ParticleKind::Dust:     p.drag = 5.0f; p.gravity = -40.0f; break;
        }
        particles_.push_back(p);
    }
}

void Fx::confetti(Vector2 at, int count, float speed, float angleDeg, float spreadDeg)
{
    for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
        const float a = (angleDeg + (rand01() - 0.5f) * spreadDeg) * kDegToRad;
        const float s = speed * (0.5f + 0.5f * rand01());
        Particle p{};
        p.pos = at;
        p.vel = { std::cos(a) * s, std::sin(a) * s };
        p.maxLife = p.life = 1.6f + 1.2f * rand01();
        p.size = 5.0f + 5.0f * rand01();
        p.rot = rand01() * 360.0f;
        p.spin = (rand01() - 0.5f) * 900.0f;
        p.color = Renderer::pieceColor(int(rand01() * 7.0f) % 7);
        p.kind = ParticleKind::Confetti;
        p.drag = 2.2f;
        p.gravity = 260.0f;
        particles_.push_back(p);
    }
}

void Fx::popup(const std::string& text, Vector2 at, float size, Color color, float duration, float delay, Face face,
               float rise, Slot slot)
{
    if (slot != Slot::None)
        eraseIf(popups_, [slot](const Popup& p) { return p.slot == slot; });
    popups_.push_back({ text, at, 0.0f, duration, delay, size, rise, color, face, slot });
}

void Fx::rowFlash(Rectangle area, Color color) { rowFlashes_.push_back({ area, color, 0.0f, 0.32f }); }
void Fx::trail(Rectangle area, Color color) { trails_.push_back({ area, color, 0.0f, 0.18f }); }
void Fx::cellFlash(Rectangle cell, Color color, float duration) { cellFlashes_.push_back({ cell, color, 0.0f, duration }); }
void Fx::ring(Vector2 center, float radius, Color color, float duration)
{
    rings_.push_back({ center, radius, 0.0f, duration, color });
}

void Fx::shake(float trauma) { trauma_ = std::min(1.0f, trauma_ + trauma); }

void Fx::flash(Color color, float strength)
{
    flashColor_ = color;
    flash_ = std::max(flash_, strength);
}

void Fx::collapseRows(const std::array<int, 4>& rows, int count)
{
    if (count <= 0)
        return;
    std::array<float, kBoardH> off{}, vel{};
    for (int y = 0; y < kBoardH; ++y) {
        const bool cleared = std::find(rows.begin(), rows.begin() + count, y) != rows.begin() + count;
        if (cleared)
            continue;
        int below = 0;
        for (int i = 0; i < count; ++i)
            below += rows[std::size_t(i)] > y ? 1 : 0;
        const int dst = y + below;
        off[std::size_t(dst)] = rowOff_[std::size_t(y)] - float(below);
        vel[std::size_t(dst)] = rowVel_[std::size_t(y)];
    }
    rowOff_ = off;
    rowVel_ = vel;
}

Vector2 Fx::shakeOffset() const
{
    const float amount = trauma_ * trauma_ * 16.0f * cfg::kShakeStrength;
    return { amount * (std::sin(time_ * 47.0f) * 0.6f + std::sin(time_ * 83.0f + 1.3f) * 0.4f),
             amount * (std::sin(time_ * 53.0f + 2.1f) * 0.6f + std::sin(time_ * 71.0f + 0.4f) * 0.4f) };
}

void Fx::drawWorld(Renderer& r, Pass pass) const
{
    const bool glow = pass == Pass::Glow;

    for (const Timed& t : trails_) {  // hard-drop light trails
        const float k = 1.0f - t.t / t.duration;
        const Color top = withAlpha(t.color, 0.0f), bottom = withAlpha(t.color, (glow ? 0.28f : 0.2f) * k * k);
        DrawRectangleGradientEx(t.area, top, bottom, bottom, top);
    }

    for (const Timed& f : rowFlashes_) {  // cleared-row flashes widen and fade
        const float k = f.t / f.duration;
        const float grow = 1.0f + 0.25f * k;
        Rectangle a = f.area;
        a.x -= a.width * (grow - 1.0f) * 0.5f;
        a.width *= grow;
        const float squash = a.height * (1.0f - k) * 0.5f;
        a.y += a.height * 0.5f - squash;
        a.height = std::max(2.0f, squash * 2.0f);
        DrawRectangleRec(a, withAlpha(f.color, (1.0f - k) * (glow ? 0.55f : 0.75f)));
    }

    for (const Timed& f : cellFlashes_) {  // locked-cell flashes
        const float k = 1.0f - f.t / f.duration;
        r.tile(kTileWhite, f.area, withAlpha(f.color, k * (glow ? 0.6f : 0.75f)));
    }

    for (const Ring& ring : rings_) {
        const float k = ring.t / ring.duration;
        const float radius = ring.radius * (0.2f + 0.8f * (1.0f - (1.0f - k) * (1.0f - k)));
        const float thick = 6.0f * (1.0f - k) + 1.0f;
        DrawRing(ring.center, radius - thick, radius, 0.0f, 360.0f, 64, withAlpha(ring.color, 1.0f - k));
    }

    for (const Particle& p : particles_) {
        const float k = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        switch (p.kind) {
            case ParticleKind::Spark: {
                const float len = std::min(28.0f, std::hypot(p.vel.x, p.vel.y) * 0.035f + 2.0f);
                const float sp = std::max(1.0f, std::hypot(p.vel.x, p.vel.y));
                const Vector2 tail = { p.pos.x - p.vel.x / sp * len, p.pos.y - p.vel.y / sp * len };
                DrawLineEx(tail, p.pos, p.size * 0.35f * k + 0.8f, withAlpha(p.color, k));
                if (glow)
                    r.glowDot(p.pos, p.size * 1.6f, withAlpha(p.color, k * 0.8f));
                break;
            }
            case ParticleKind::Chunk: {
                const float s = p.size * (0.5f + 0.5f * k);
                DrawRectanglePro({ p.pos.x, p.pos.y, s, s }, { s * 0.5f, s * 0.5f }, p.rot,
                                 withAlpha(glow ? p.color : ColorBrightness(p.color, 0.15f), k));
                break;
            }
            case ParticleKind::Confetti: {
                const float flip = std::fabs(std::sin(p.rot * kDegToRad * 2.0f));
                DrawRectanglePro({ p.pos.x, p.pos.y, p.size, p.size * 0.45f * (0.25f + 0.75f * flip) },
                                 { p.size * 0.5f, p.size * 0.2f }, p.rot, withAlpha(p.color, std::min(1.0f, k * 2.0f)));
                break;
            }
            case ParticleKind::Dust:
                r.glowDot(p.pos, p.size * (1.5f - 0.5f * k), withAlpha(p.color, k * (glow ? 0.35f : 0.5f)));
                break;
        }
    }
}

void Fx::drawPopups(Renderer& r, Pass pass) const
{
    for (const Popup& p : popups_) {
        const float t = p.t - p.delay;
        if (t < 0.0f)
            continue;
        const float appear = std::min(1.0f, t / 0.22f);
        const float scale = 0.4f + 0.6f * easeOutBack(appear);
        const float fadeOut = std::clamp((p.duration - t) / 0.35f, 0.0f, 1.0f);
        const float alpha = std::min(appear * 1.5f, 1.0f) * fadeOut;
        const float size = p.size * scale;
        const Vector2 pos = { p.pos.x, p.pos.y - p.rise * (t / p.duration) - size * 0.5f };
        // rasterized once at the final size, scaled while it pops in
        if (pass == Pass::Glow) {
            r.text(p.face, p.text, pos, size, withAlpha(p.color, alpha * 0.9f), Align::Center, size * 0.06f, p.size);
        } else {
            r.text(p.face, p.text, { pos.x + 2.0f, pos.y + 3.0f }, size, withAlpha(BLACK, alpha * 0.45f), Align::Center,
                   size * 0.06f, p.size);
            r.text(p.face, p.text, pos, size, withAlpha(ColorLerp(p.color, WHITE, 0.35f), alpha), Align::Center,
                   size * 0.06f, p.size);
        }
    }
}

void Fx::drawScreenFlash(Renderer& r) const
{
    if (flash_ <= 0.0f)
        return;
    DrawRectangle(0, 0, r.width(), r.height(), withAlpha(flashColor_, flash_ * 0.55f));
}
