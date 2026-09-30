#include "render.hpp"

#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace {

constexpr int kTilePx = 128;  // tiles are shaded at high resolution, then mipmapped
constexpr int kTilePad = 8;
constexpr int kTileStride = kTilePx + 2 * kTilePad;

// Separable 9-tap Gaussian (5 bilinear fetches)
const char* kBlurFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 direction;
out vec4 finalColor;
void main()
{
    vec2 o1 = direction * 1.3846153846;
    vec2 o2 = direction * 3.2307692308;
    vec3 c = texture(texture0, fragTexCoord).rgb * 0.2270270270;
    c += texture(texture0, fragTexCoord + o1).rgb * 0.3162162162;
    c += texture(texture0, fragTexCoord - o1).rgb * 0.3162162162;
    c += texture(texture0, fragTexCoord + o2).rgb * 0.0702702703;
    c += texture(texture0, fragTexCoord - o2).rgb * 0.0702702703;
    finalColor = vec4(c, 1.0);
}
)";

struct Vec3 {
    float r, g, b;
};
Vec3 mix(Vec3 a, Vec3 b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
Vec3 mul(Vec3 a, float k) { return { a.r * k, a.g * k, a.b * k }; }

float smooth01(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float sdRoundBox(float px, float py, float half, float radius)
{
    const float qx = std::fabs(px) - half + radius, qy = std::fabs(py) - half + radius;
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
}

unsigned char to8(float v) { return (unsigned char)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f); }

// Per-pixel shading of one block tile: rounded square, lit bevel, gloss and a bright rim
Color shadeTile(int tile, float x, float y)
{
    constexpr float S = kTilePx;
    const float c = S * 0.5f, half = S * 0.5f - S * 0.035f, radius = S * 0.18f;
    auto sd = [&](float px, float py) { return sdRoundBox(px - c, py - c, half, radius); };
    const float d = sd(x, y);
    const float cover = std::clamp(0.5f - d, 0.0f, 1.0f);
    const float e = -d;  // depth inside the shape, in pixels

    if (tile == kTileWhite)
        return { 255, 255, 255, to8(cover) };
    if (tile == kTileGhost) {
        const float ring = 1.0f - smooth01(S * 0.035f, S * 0.075f, e);
        return { 255, 255, 255, to8(cover * std::max(ring, 0.16f)) };
    }

    const Color bc = Renderer::pieceColor(tile);
    const Vec3 base = { bc.r / 255.0f, bc.g / 255.0f, bc.b / 255.0f };
    if (cover <= 0.0f)
        return { bc.r, bc.g, bc.b, 0 };  // keep the colour so mipmaps don't bleed white

    float nx = sd(x + 0.5f, y) - sd(x - 0.5f, y), ny = sd(x, y + 0.5f) - sd(x, y - 0.5f);
    const float len = std::sqrt(nx * nx + ny * ny);
    if (len > 1e-5f) {
        nx /= len;
        ny /= len;
    }
    const float bevelW = S * 0.13f;
    const float bevel = 1.0f - smooth01(0.0f, bevelW, e);
    const float light = nx * -0.55f + ny * -0.83f;  // > 0 on edges facing the top-left light

    Vec3 col = mul(base, 1.06f - 0.26f * (y / S));  // face: lighter at the top
    if (light > 0.0f)
        col = mix(col, { 1, 1, 1 }, light * bevel * 0.6f);
    else
        col = mul(col, 1.0f + light * bevel * 0.55f);
    const float groove = std::clamp(1.0f - std::fabs(e - bevelW) / 1.6f, 0.0f, 1.0f);
    col = mul(col, 1.0f - groove * 0.12f);
    const float gx = (x - S * 0.40f) / (S * 0.26f), gy = (y - S * 0.30f) / (S * 0.11f);
    const float gloss = std::clamp(1.0f - (gx * gx + gy * gy), 0.0f, 1.0f);
    col = mix(col, { 1, 1, 1 }, gloss * gloss * 0.38f * smooth01(bevelW * 0.9f, bevelW * 1.4f, e));
    const float rim = 1.0f - smooth01(0.0f, 2.2f, e);
    col = mix(col, mix(base, { 1, 1, 1 }, 0.55f), rim * 0.7f);
    return { to8(col.r), to8(col.g), to8(col.b), to8(cover) };
}

Image buildAtlas()
{
    const int w = kTileCount * kTileStride, h = kTileStride;
    Image img = GenImageColor(w, h, BLANK);
    Color* px = static_cast<Color*>(img.data);
    for (int t = 0; t < kTileCount; ++t) {
        const Color edge = (t < kTileWhite) ? Renderer::pieceColor(t) : WHITE;
        for (int y = 0; y < h; ++y)  // transparent padding carries the tile colour too
            for (int x = 0; x < kTileStride; ++x)
                px[y * w + t * kTileStride + x] = { edge.r, edge.g, edge.b, 0 };
        for (int y = 0; y < kTilePx; ++y)
            for (int x = 0; x < kTilePx; ++x)
                px[(kTilePad + y) * w + t * kTileStride + kTilePad + x] = shadeTile(t, x + 0.5f, y + 0.5f);
    }
    return img;
}

// Soft round light: smooth falloff to zero at the edge
Image buildRadial(int size)
{
    Image img = GenImageColor(size, size, BLANK);
    Color* px = static_cast<Color*>(img.data);
    const float c = size * 0.5f;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const float dx = (x + 0.5f - c) / c, dy = (y + 0.5f - c) / c;
            const float r2 = std::min(1.0f, dx * dx + dy * dy);
            const float a = (1.0f - r2) * (1.0f - r2);
            px[y * size + x] = { 255, 255, 255, to8(a) };
        }
    return img;
}

std::string windowsFont(std::initializer_list<const char*> files)
{
    const char* windir = std::getenv("WINDIR");
    const std::string dir = std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\";
    for (const char* f : files) {
        const std::string path = dir + f;
        if (FileExists(path.c_str()))
            return path;
    }
    return {};
}

float hash01(int i)
{
    const float h = std::sin(float(i) * 12.9898f + 78.233f) * 43758.5453f;
    return h - std::floor(h);
}

Rectangle flipped(const RenderTexture2D& rt)
{
    return { 0, 0, float(rt.texture.width), -float(rt.texture.height) };
}

Color grey(float v)
{
    const unsigned char c = to8(v);
    return { c, c, c, 255 };
}

} // namespace

std::string fmt(const char* format, ...)
{
    char buf[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    return buf;
}

std::string withCommas(long long value)
{
    const std::string digits = std::to_string(value < 0 ? -value : value);
    std::string out;
    int n = 0;
    for (int i = int(digits.size()) - 1; i >= 0; --i) {
        out.insert(out.begin(), digits[std::size_t(i)]);
        if (++n % 3 == 0 && i > 0)
            out.insert(out.begin(), ',');
    }
    return value < 0 ? "-" + out : out;
}

std::string clockTime(double seconds, bool centis)
{
    seconds = std::max(0.0, seconds);
    const int total = int(seconds);
    const int cs = std::min(99, int((seconds - total) * 100.0));
    return centis ? fmt("%d:%02d.%02d", total / 60, total % 60, cs) : fmt("%d:%02d", total / 60, total % 60);
}

Color Renderer::pieceColor(int colorIndex)
{
    const cfg::Rgb c = (colorIndex >= 0 && colorIndex < 7) ? cfg::kPieceColors[colorIndex] : cfg::kGreyColor;
    return { c.r, c.g, c.b, 255 };
}

bool Renderer::init()
{
    facePath_[int(Face::Display)] = windowsFont({ "bahnschrift.ttf", "segoeui.ttf", "arial.ttf" });
    facePath_[int(Face::Bold)] = windowsFont({ "segoeuib.ttf", "arialbd.ttf", "arial.ttf" });
    facePath_[int(Face::Ui)] = windowsFont({ "seguisb.ttf", "segoeui.ttf", "arial.ttf" });

    Image atlas = buildAtlas();
    atlas_ = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    GenTextureMipmaps(&atlas_);
    SetTextureFilter(atlas_, TEXTURE_FILTER_TRILINEAR);

    Image radial = buildRadial(128);
    radial_ = LoadTextureFromImage(radial);
    UnloadImage(radial);
    SetTextureFilter(radial_, TEXTURE_FILTER_BILINEAR);

    blurShader_ = LoadShaderFromMemory(nullptr, kBlurFs);
    bloomOk_ = IsShaderValid(blurShader_);
    if (bloomOk_)
        blurDirLoc_ = GetShaderLocation(blurShader_, "direction");

    beginFrame(0.0f);
    return true;
}

void Renderer::shutdown()
{
    clearFonts();
    freeTargets();
    if (bloomOk_)
        UnloadShader(blurShader_);
    UnloadTexture(atlas_);
    UnloadTexture(radial_);
}

void Renderer::beginFrame(float dt)
{
    const int w = std::max(1, GetScreenWidth()), h = std::max(1, GetScreenHeight());
    if (w != winW_ || h != winH_) {
        const bool first = winW_ == 0;
        winW_ = w;
        winH_ = h;
        scale_ = std::min(w / kVirtualW, h / kVirtualH);
        offset_ = { (w - kVirtualW * scale_) * 0.5f, (h - kVirtualH * scale_) * 0.5f };
        if (first) {
            fontScale_ = scale_;
            allocTargets();
        } else {
            resizeTimer_ = 0.2f;  // rebuild fonts and buffers once resizing settles
        }
    }
    if (resizeTimer_ > 0.0f) {
        resizeTimer_ -= dt;
        if (resizeTimer_ <= 0.0f) {
            resizeTimer_ = 0.0f;
            if (fontScale_ != scale_) {
                // re-rasterize every font in use at the new scale right away
                std::vector<std::pair<Face, float>> inUse;
                for (const auto& [key, cached] : fonts_)
                    inUse.emplace_back(Face(key.first), cached.size);
                clearFonts();
                fontScale_ = scale_;
                for (const auto& [face, size] : inUse)
                    font(face, size);
            }
            allocTargets();
        }
    }
}

Camera2D Renderer::camera(Vector2 shake) const
{
    Camera2D cam{};
    cam.offset = { offset_.x + shake.x * scale_, offset_.y + shake.y * scale_ };
    cam.target = { 0, 0 };
    cam.rotation = 0.0f;
    cam.zoom = scale_;
    return cam;
}

Vector2 Renderer::toVirtual(Vector2 screen) const
{
    return { (screen.x - offset_.x) / scale_, (screen.y - offset_.y) / scale_ };
}

const Font& Renderer::font(Face face, float size)
{
    const int px = std::max(6, int(std::lround(size * fontScale_)));
    const auto key = std::make_pair(int(face), px);
    if (auto it = fonts_.find(key); it != fonts_.end())
        return it->second.font;
    Font f{};
    const std::string& path = facePath_[int(face)];
    if (!path.empty())
        f = LoadFontEx(path.c_str(), px, nullptr, 0);
    if (!IsFontValid(f) || f.texture.id == 0)
        f = GetFontDefault();
    else
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return fonts_.emplace(key, CachedFont{ f, size }).first->second.font;
}

void Renderer::clearFonts()
{
    const unsigned defaultTex = GetFontDefault().texture.id;
    for (auto& [key, cached] : fonts_)
        if (cached.font.texture.id != defaultTex)
            UnloadFont(cached.font);
    fonts_.clear();
    digitW_.clear();
}

float Renderer::digitWidth(Face face, float size)
{
    const int px = std::max(6, int(std::lround(size * fontScale_)));
    const auto key = std::make_pair(int(face), px);
    if (auto it = digitW_.find(key); it != digitW_.end())
        return it->second;
    const Font& f = font(face, size);
    float w = 0.0f;
    for (char c = '0'; c <= '9'; ++c) {
        const char s[2] = { c, 0 };
        w = std::max(w, MeasureTextEx(f, s, size, 0.0f).x);
    }
    digitW_[key] = w;
    return w;
}

void Renderer::text(Face face, const std::string& s, Vector2 pos, float size, Color color, Align align, float spacing,
                    float rasterSize)
{
    const Font& f = font(face, rasterSize > 0.0f ? rasterSize : size);
    if (align != Align::Left) {
        const float w = MeasureTextEx(f, s.c_str(), size, spacing).x;
        pos.x -= align == Align::Center ? w * 0.5f : w;
    }
    // snap to whole screen pixels so glyphs stay crisp
    pos.x = (std::round(pos.x * scale_ + offset_.x) - offset_.x) / scale_;
    pos.y = (std::round(pos.y * scale_ + offset_.y) - offset_.y) / scale_;
    DrawTextEx(f, s.c_str(), pos, size, spacing, color);
}

void Renderer::textDigits(Face face, const std::string& s, Vector2 pos, float size, Color color, Align align)
{
    const Font& f = font(face, size);
    const float dw = digitWidth(face, size);
    auto advance = [&](char c) {
        if (c >= '0' && c <= '9')
            return dw;
        const char str[2] = { c, 0 };
        return MeasureTextEx(f, str, size, 0.0f).x;
    };
    float total = 0.0f;
    for (char c : s)
        total += advance(c);
    if (align == Align::Center)
        pos.x -= total * 0.5f;
    else if (align == Align::Right)
        pos.x -= total;
    pos.y = (std::round(pos.y * scale_ + offset_.y) - offset_.y) / scale_;
    for (char c : s) {
        const float a = advance(c);
        float x = pos.x;
        if (c >= '0' && c <= '9') {
            const char str[2] = { c, 0 };
            x += (dw - MeasureTextEx(f, str, size, 0.0f).x) * 0.5f;
        }
        x = (std::round(x * scale_ + offset_.x) - offset_.x) / scale_;
        DrawTextCodepoint(f, c, { x, pos.y }, size, color);
        pos.x += a;
    }
}

Vector2 Renderer::measure(Face face, const std::string& s, float size, float spacing)
{
    return MeasureTextEx(font(face, size), s.c_str(), size, spacing);
}

void Renderer::tile(int index, Rectangle dst, Color tint) const
{
    const Rectangle src = { float(index * kTileStride + kTilePad), float(kTilePad), float(kTilePx), float(kTilePx) };
    DrawTexturePro(atlas_, src, dst, { 0, 0 }, 0.0f, tint);
}

void Renderer::glowDot(Vector2 center, float radius, Color color) const
{
    DrawTexturePro(radial_, { 0, 0, float(radial_.width), float(radial_.height) },
                   { center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f }, { 0, 0 }, 0.0f, color);
}

void Renderer::allocTargets()
{
    freeTargets();
    auto make = [](int w, int h) {
        RenderTexture2D rt = LoadRenderTexture(std::max(1, w), std::max(1, h));
        SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
        return rt;
    };
    backdrop_ = make(winW_ / 4, winH_ / 4);
    if (!bloomOk_)
        return;
    glow_ = make(winW_ / 2, winH_ / 2);
    blurA_ = make(winW_ / 4, winH_ / 4);
    blurB_ = make(winW_ / 4, winH_ / 4);
    blurC_ = make(winW_ / 8, winH_ / 8);
    blurD_ = make(winW_ / 8, winH_ / 8);
}

void Renderer::freeTargets()
{
    for (RenderTexture2D* rt : { &glow_, &blurA_, &blurB_, &blurC_, &blurD_, &backdrop_ }) {
        if (rt->id != 0)
            UnloadRenderTexture(*rt);
        *rt = RenderTexture2D{};
    }
}

void Renderer::beginGlow(Vector2 shake)
{
    BeginTextureMode(glow_);
    ClearBackground(BLANK);
    const float k = float(glow_.texture.width) / float(winW_);
    Camera2D cam = camera(shake);
    cam.offset.x *= k;
    cam.offset.y *= k;
    cam.zoom *= k;
    BeginMode2D(cam);
    BeginBlendMode(BLEND_ADDITIVE);
}

void Renderer::blur(RenderTexture2D& src, RenderTexture2D& dst, float dx, float dy) const
{
    const Vector2 dir = { dx / float(src.texture.width), dy / float(src.texture.height) };
    BeginTextureMode(dst);
    ClearBackground(BLANK);
    BeginShaderMode(blurShader_);
    SetShaderValue(blurShader_, blurDirLoc_, &dir, SHADER_UNIFORM_VEC2);
    BeginBlendMode(BLEND_ADD_COLORS);
    DrawTexturePro(src.texture, flipped(src), { 0, 0, float(dst.texture.width), float(dst.texture.height) }, { 0, 0 },
                   0.0f, WHITE);
    EndBlendMode();
    EndShaderMode();
    EndTextureMode();
}

void Renderer::endGlow()
{
    EndBlendMode();
    EndMode2D();
    EndTextureMode();

    auto downsample = [](RenderTexture2D& src, RenderTexture2D& dst) {
        BeginTextureMode(dst);
        ClearBackground(BLANK);
        BeginBlendMode(BLEND_ADD_COLORS);
        DrawTexturePro(src.texture, flipped(src), { 0, 0, float(dst.texture.width), float(dst.texture.height) },
                       { 0, 0 }, 0.0f, WHITE);
        EndBlendMode();
        EndTextureMode();
    };
    downsample(glow_, blurA_);
    blur(blurA_, blurB_, 1.0f, 0.0f);
    blur(blurB_, blurA_, 0.0f, 1.0f);
    downsample(blurA_, blurC_);
    for (int i = 0; i < 2; ++i) {
        blur(blurC_, blurD_, 1.0f, 0.0f);
        blur(blurD_, blurC_, 0.0f, 1.0f);
    }

    // Fold the tight and the wide glow into one texture: compositing is then a single pass
    BeginTextureMode(blurB_);
    ClearBackground(BLANK);
    BeginBlendMode(BLEND_ADD_COLORS);
    const Rectangle dst = { 0, 0, float(blurB_.texture.width), float(blurB_.texture.height) };
    DrawTexturePro(blurA_.texture, flipped(blurA_), dst, { 0, 0 }, 0.0f, grey(0.75f));
    DrawTexturePro(blurC_.texture, flipped(blurC_), dst, { 0, 0 }, 0.0f, grey(0.80f));
    EndBlendMode();
    EndTextureMode();
}

void Renderer::compositeGlow(float strength) const
{
    if (!bloomOk_ || strength <= 0.0f)
        return;
    BeginBlendMode(BLEND_ADD_COLORS);
    DrawTexturePro(blurB_.texture, flipped(blurB_), { 0, 0, float(winW_), float(winH_) }, { 0, 0 }, 0.0f, grey(strength));
    EndBlendMode();
}

void Renderer::renderBackdrop(float time, float hue, float danger)
{
    const int bw = backdrop_.texture.width, bh = backdrop_.texture.height;
    const float w = float(bw), h = float(bh);
    Color top = ColorFromHSV(hue, 0.70f, 0.17f);
    Color bottom = ColorFromHSV(std::fmod(hue + 50.0f, 360.0f), 0.80f, 0.05f);
    if (danger > 0.0f) {
        top = ColorLerp(top, { 80, 8, 20, 255 }, danger * 0.8f);
        bottom = ColorLerp(bottom, { 30, 0, 8, 255 }, danger * 0.6f);
    }
    BeginTextureMode(backdrop_);
    DrawRectangleGradientV(0, 0, bw, bh, top, bottom);

    BeginBlendMode(BLEND_ADDITIVE);
    const float big = std::max(w, h);
    for (int i = 0; i < 6; ++i) {  // slow drifting colour blobs
        const float t = time * 0.045f + float(i) * 1.9f;
        const Vector2 c = { w * (0.5f + 0.45f * std::sin(t * 0.8f + float(i) * 1.3f)),
                            h * (0.5f + 0.42f * std::cos(t * 0.6f + float(i) * 2.4f)) };
        Color col = ColorFromHSV(std::fmod(hue + float(i) * 32.0f, 360.0f), 0.75f, 1.0f);
        col.a = 26;
        glowDot(c, big * (0.26f + 0.07f * std::sin(t * 1.7f + float(i))), col);
    }
    EndBlendMode();

    // vignette
    const int vw = int(w * 0.18f), vh = int(h * 0.2f);
    DrawRectangleGradientH(0, 0, vw, bh, { 0, 0, 0, 150 }, BLANK);
    DrawRectangleGradientH(bw - vw, 0, vw, bh, BLANK, { 0, 0, 0, 150 });
    DrawRectangleGradientV(0, 0, bw, vh, { 0, 0, 0, 110 }, BLANK);
    DrawRectangleGradientV(0, bh - vh, bw, vh, BLANK, { 0, 0, 0, 140 });
    EndTextureMode();
}

void Renderer::drawBackdrop(float time, float hue) const
{
    const float w = float(winW_), h = float(winH_);
    DrawTexturePro(backdrop_.texture, flipped(backdrop_), { 0, 0, w, h }, { 0, 0 }, 0.0f, WHITE);

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 90; ++i) {  // rising sparkles
        const float speed = 0.004f + 0.012f * hash01(i * 3);
        const float y = std::fmod(hash01(i * 2 + 1) - time * speed + 10.0f, 1.0f);
        const float x = hash01(i * 2) + 0.01f * std::sin(time * 0.5f + float(i));
        const float twinkle = 0.5f + 0.5f * std::sin(time * (1.0f + hash01(i * 5) * 2.0f) + float(i));
        const float size = (1.0f + 2.0f * hash01(i * 7)) * std::max(1.0f, scale_);
        Color c = ColorFromHSV(std::fmod(hue + hash01(i * 11) * 80.0f, 360.0f), 0.35f, 1.0f);
        c.a = (unsigned char)(30.0f + 110.0f * twinkle);
        DrawRectangleV({ x * w, y * h }, { size, size }, c);
    }
    EndBlendMode();
}
