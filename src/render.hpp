#pragma once
// Drawing toolkit: virtual canvas, fonts, block tiles, bloom and background.
#include "raylib.h"

#include <map>
#include <string>
#include <utility>

// Everything is laid out on a 1280x720 virtual canvas, scaled and letterboxed to the window.
inline constexpr float kVirtualW = 1280.0f;
inline constexpr float kVirtualH = 720.0f;

enum class Face { Display, Bold, Ui };      // Bahnschrift, Segoe UI Bold, Segoe UI Semibold
enum class Align { Left, Center, Right };
enum class Pass { Main, Glow };             // glow pass = what feeds the bloom

// Tile atlas entries: 0..6 = pieces (I O T S Z J L), then:
inline constexpr int kTileGrey  = 7;
inline constexpr int kTileWhite = 8;  // flat white rounded square (flashes, glow sources)
inline constexpr int kTileGhost = 9;  // white outline (tinted with the piece color)
inline constexpr int kTileCount = 10;

std::string fmt(const char* format, ...);
std::string withCommas(long long value);
std::string clockTime(double seconds, bool centis);  // m:ss.cc

class Renderer {
public:
    bool init();
    void shutdown();
    void beginFrame(float dt);  // follows window resizes

    // --- view ---
    float scale() const { return scale_; }
    Camera2D camera(Vector2 shake = { 0, 0 }) const;
    Vector2 toVirtual(Vector2 screen) const;
    int width() const { return winW_; }
    int height() const { return winH_; }

    // --- text ---
    // rasterSize: font size to rasterize at when `size` is animating (avoids a font load per frame)
    void text(Face face, const std::string& s, Vector2 pos, float size, Color color,
              Align align = Align::Left, float spacing = 0.0f, float rasterSize = 0.0f);
    void prewarm(Face face, float size) { font(face, size); }
    void textDigits(Face face, const std::string& s, Vector2 pos, float size, Color color,
                    Align align = Align::Left);  // fixed-width digits: numbers never jitter
    Vector2 measure(Face face, const std::string& s, float size, float spacing = 0.0f);

    // --- blocks ---
    void tile(int index, Rectangle dst, Color tint = WHITE) const;
    static Color pieceColor(int colorIndex);  // 0..6 pieces, 7 grey
    void glowDot(Vector2 center, float radius, Color color) const;  // soft radial light

    // --- bloom ---
    bool bloomSupported() const { return bloomOk_; }
    void beginGlow(Vector2 shake);  // draws in virtual coordinates now feed the glow buffer
    void endGlow();                 // blurs the glow buffer
    void compositeGlow(float strength) const;

    // --- backdrop (screen space) ---
    // The soft gradient, colour blobs and vignette are rendered at quarter resolution
    // (they are blurry anyway) and upscaled in one pass; sparkles are drawn at full resolution.
    void renderBackdrop(float time, float hue, float danger);  // call before BeginDrawing
    void drawBackdrop(float time, float hue) const;           // call first inside BeginDrawing

private:
    const Font& font(Face face, float size);
    float digitWidth(Face face, float size);
    void clearFonts();
    void allocTargets();
    void freeTargets();
    void blur(RenderTexture2D& src, RenderTexture2D& dst, float dx, float dy) const;

    int winW_ = 0, winH_ = 0;
    float scale_ = 1.0f;
    Vector2 offset_{ 0, 0 };
    float fontScale_ = 0.0f;
    float resizeTimer_ = 0.0f;

    struct CachedFont {
        Font font;
        float size;  // virtual size it was requested at
    };
    std::string facePath_[3];
    std::map<std::pair<int, int>, CachedFont> fonts_;  // (face, pixel size) -> font
    std::map<std::pair<int, int>, float> digitW_;

    Texture2D atlas_{};
    Texture2D radial_{};

    bool bloomOk_ = false;
    Shader blurShader_{};
    int blurDirLoc_ = -1;
    RenderTexture2D glow_{}, blurA_{}, blurB_{}, blurC_{}, blurD_{};
    RenderTexture2D backdrop_{};
};
