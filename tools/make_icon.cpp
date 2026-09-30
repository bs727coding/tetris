// Generates the multi-resolution application icon (assets/tetris.ico).
// Run with `mingw32-make icon`. Optional 2nd argument: a PNG preview of every size.
//
// The art is drawn per pixel at each size (signed-distance shapes, so every size is
// anti-aliased and the blocks snap to whole pixels): a glossy purple T tetromino, shaded
// like the in-game blocks, glowing on a dark rounded tile.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct Col {
    float r, g, b;
};
Col operator*(Col c, float k) { return { c.r * k, c.g * k, c.b * k }; }
Col operator+(Col a, Col b) { return { a.r + b.r, a.g + b.g, a.b + b.b }; }
Col mix(Col a, Col b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

constexpr Col kPurple{ 186 / 255.0f, 80 / 255.0f, 1.0f };
constexpr Col kWhite{ 1, 1, 1 };

float smooth01(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float sdRoundBox(float px, float py, float hx, float hy, float radius)
{
    const float qx = std::fabs(px) - hx + radius, qy = std::fabs(py) - hy + radius;
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
}

// Same look as the game's block tiles: lit bevel, gloss and a bright rim (see render.cpp)
Col shadeBlock(float x, float y, float size, float& cover)
{
    const float c = size * 0.5f, half = size * 0.5f - std::max(0.5f, size * 0.035f), radius = size * 0.18f;
    auto sd = [&](float px, float py) { return sdRoundBox(px - c, py - c, half, half, radius); };
    const float d = sd(x, y);
    cover = std::clamp(0.5f - d, 0.0f, 1.0f);
    const float e = -d;
    float nx = sd(x + 0.5f, y) - sd(x - 0.5f, y), ny = sd(x, y + 0.5f) - sd(x, y - 0.5f);
    const float len = std::sqrt(nx * nx + ny * ny);
    if (len > 1e-5f) {
        nx /= len;
        ny /= len;
    }
    const float bevelW = std::max(0.8f, size * 0.13f);
    const float bevel = 1.0f - smooth01(0.0f, bevelW, e);
    const float light = nx * -0.55f + ny * -0.83f;
    Col col = kPurple * (1.06f - 0.26f * (y / size));
    col = light > 0.0f ? mix(col, kWhite, light * bevel * 0.6f) : col * (1.0f + light * bevel * 0.55f);
    const float gx = (x - size * 0.40f) / (size * 0.26f), gy = (y - size * 0.30f) / (size * 0.11f);
    const float gloss = std::clamp(1.0f - (gx * gx + gy * gy), 0.0f, 1.0f);
    col = mix(col, kWhite, gloss * gloss * (size < 10.0f ? 0.2f : 0.38f) * smooth01(bevelW * 0.9f, bevelW * 1.4f, e));
    const float rim = 1.0f - smooth01(0.0f, std::max(0.8f, size * 0.017f), e);
    return mix(col, mix(kPurple, kWhite, 0.55f), rim * 0.7f);
}

// RGBA8, straight alpha, rows top to bottom
std::vector<std::uint8_t> renderIcon(int s)
{
    const float fs = float(s);
    const float margin = std::max(0.5f, fs * 0.03f);
    const float plateHalf = fs * 0.5f - margin, plateRadius = fs * 0.2f;

    // The T: blocks on whole pixels so small sizes stay crisp
    const int block = std::max(4, int(std::lround(fs * 0.235f)));
    const int ox = (s - 3 * block) / 2, oy = (s - 2 * block) / 2;
    const int cells[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } };
    const float tcx = float(ox) + 1.5f * float(block), tcy = float(oy) + 1.0f * float(block);
    const float glow = s <= 32 ? 0.5f : 1.0f;  // small sizes need contrast more than glow

    std::vector<std::uint8_t> px(std::size_t(s) * std::size_t(s) * 4);
    for (int y = 0; y < s; ++y)
        for (int x = 0; x < s; ++x) {
            const float fx = float(x) + 0.5f, fy = float(y) + 0.5f;
            const float dPlate = sdRoundBox(fx - fs * 0.5f, fy - fs * 0.5f, plateHalf, plateHalf, plateRadius);
            const float plateCover = std::clamp(0.5f - dPlate, 0.0f, 1.0f);

            // plate: dark indigo gradient with a purple glow behind the piece
            Col col = mix(Col{ 0.13f, 0.08f, 0.29f }, Col{ 0.04f, 0.04f, 0.12f }, fy / fs);
            const float gr = std::hypot(fx - tcx, fy - tcy) / (fs * 0.5f);
            col = col + kPurple * (0.35f * glow * std::exp(-gr * gr * 2.5f));

            // tight halo hugging the T
            float dT = 1e9f;
            for (const auto& c : cells) {
                const float bx = float(ox + c[0] * block) + float(block) * 0.5f;
                const float by = float(oy + c[1] * block) + float(block) * 0.5f;
                dT = std::min(dT, sdRoundBox(fx - bx, fy - by, float(block) * 0.5f, float(block) * 0.5f, 0.0f));
            }
            col = col + kPurple * (0.55f * glow * std::exp(-std::max(dT, 0.0f) / (fs * 0.05f)));

            // thin light rim around the plate
            const float rimW = std::max(1.0f, fs * 0.012f);
            col = mix(col, Col{ 0.62f, 0.52f, 1.0f }, 0.55f * (1.0f - smooth01(0.0f, rimW, -dPlate)));

            // blocks
            for (const auto& c : cells) {
                const float lx = fx - float(ox + c[0] * block), ly = fy - float(oy + c[1] * block);
                if (lx < 0.0f || ly < 0.0f || lx >= float(block) || ly >= float(block))
                    continue;
                float cover = 0.0f;
                const Col b = shadeBlock(lx, ly, float(block), cover);
                col = mix(col, b, cover);
            }

            std::uint8_t* p = &px[(std::size_t(y) * std::size_t(s) + std::size_t(x)) * 4];
            p[0] = std::uint8_t(std::lround(std::clamp(col.r, 0.0f, 1.0f) * 255.0f));
            p[1] = std::uint8_t(std::lround(std::clamp(col.g, 0.0f, 1.0f) * 255.0f));
            p[2] = std::uint8_t(std::lround(std::clamp(col.b, 0.0f, 1.0f) * 255.0f));
            p[3] = std::uint8_t(std::lround(plateCover * 255.0f));
        }
    return px;
}

void put16(std::vector<std::uint8_t>& out, unsigned v) { out.push_back(std::uint8_t(v)); out.push_back(std::uint8_t(v >> 8)); }
void put32(std::vector<std::uint8_t>& out, unsigned v) { put16(out, v & 0xFFFF); put16(out, v >> 16); }

std::vector<std::uint8_t> encodePng(const std::vector<std::uint8_t>& rgba, int w, int h)
{
    std::vector<std::uint8_t> out;
    stbi_write_png_to_func([](void* ctx, void* data, int size) {
            auto* v = static_cast<std::vector<std::uint8_t>*>(ctx);
            v->insert(v->end(), static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + size);
        }, &out, w, h, 4, rgba.data(), w * 4);
    return out;
}

// Classic icon image: BITMAPINFOHEADER + bottom-up BGRA rows + 1-bit AND mask
std::vector<std::uint8_t> encodeDib(const std::vector<std::uint8_t>& rgba, int s)
{
    const unsigned maskStride = unsigned((s + 31) / 32) * 4;
    std::vector<std::uint8_t> out;
    put32(out, 40);
    put32(out, unsigned(s));
    put32(out, unsigned(s * 2));  // colour + mask
    put16(out, 1);
    put16(out, 32);
    put32(out, 0);
    put32(out, unsigned(s * s * 4) + maskStride * unsigned(s));
    for (int i = 0; i < 4; ++i)
        put32(out, 0);
    for (int y = s - 1; y >= 0; --y)
        for (int x = 0; x < s; ++x) {
            const std::uint8_t* p = &rgba[(std::size_t(y) * std::size_t(s) + std::size_t(x)) * 4];
            out.insert(out.end(), { p[2], p[1], p[0], p[3] });
        }
    for (int y = s - 1; y >= 0; --y) {
        std::vector<std::uint8_t> row(maskStride, 0);
        for (int x = 0; x < s; ++x)
            if (rgba[(std::size_t(y) * std::size_t(s) + std::size_t(x)) * 4 + 3] == 0)
                row[std::size_t(x / 8)] |= std::uint8_t(0x80 >> (x % 8));
        out.insert(out.end(), row.begin(), row.end());
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: make_icon <out.ico> [preview.png]\n");
        return 1;
    }
    const int sizes[] = { 16, 20, 24, 32, 40, 48, 64, 96, 128, 256 };
    constexpr int kCount = int(std::size(sizes));

    std::vector<std::vector<std::uint8_t>> images;
    std::vector<std::vector<std::uint8_t>> pixels;
    for (int s : sizes) {
        pixels.push_back(renderIcon(s));
        images.push_back(s >= 256 ? encodePng(pixels.back(), s, s) : encodeDib(pixels.back(), s));
    }

    std::vector<std::uint8_t> ico;
    put16(ico, 0);
    put16(ico, 1);  // type: icon
    put16(ico, kCount);
    unsigned offset = 6 + 16 * kCount;
    for (int i = 0; i < kCount; ++i) {
        ico.push_back(std::uint8_t(sizes[i] >= 256 ? 0 : sizes[i]));
        ico.push_back(std::uint8_t(sizes[i] >= 256 ? 0 : sizes[i]));
        ico.push_back(0);
        ico.push_back(0);
        put16(ico, 1);
        put16(ico, 32);
        put32(ico, unsigned(images[std::size_t(i)].size()));
        put32(ico, offset);
        offset += unsigned(images[std::size_t(i)].size());
    }
    for (const auto& img : images)
        ico.insert(ico.end(), img.begin(), img.end());

    FILE* f = std::fopen(argv[1], "wb");
    if (!f || std::fwrite(ico.data(), 1, ico.size(), f) != ico.size()) {
        std::fprintf(stderr, "could not write %s\n", argv[1]);
        return 1;
    }
    std::fclose(f);
    std::printf("wrote %s (%d sizes, %zu bytes)\n", argv[1], kCount, ico.size());

    if (argc >= 3) {  // preview: every size side by side, small ones enlarged 4x (nearest neighbour)
        int w = 16, h = 0;
        for (int i = 0; i < kCount; ++i) {
            const int k = sizes[i] <= 48 ? 4 : 1;
            w += sizes[i] * k + 16;
            h = std::max(h, sizes[i] * k);
        }
        h += 32;
        std::vector<std::uint8_t> sheet(std::size_t(w) * std::size_t(h) * 4, 0);
        for (std::size_t p = 0; p < sheet.size(); p += 4) {  // mid-grey backdrop shows the edges
            sheet[p] = sheet[p + 1] = sheet[p + 2] = 110;
            sheet[p + 3] = 255;
        }
        int x0 = 16;
        for (int i = 0; i < kCount; ++i) {
            const int s = sizes[i], k = s <= 48 ? 4 : 1;
            for (int y = 0; y < s * k; ++y)
                for (int x = 0; x < s * k; ++x) {
                    const std::uint8_t* src = &pixels[std::size_t(i)][(std::size_t(y / k) * std::size_t(s) + std::size_t(x / k)) * 4];
                    std::uint8_t* dst = &sheet[(std::size_t(y + 16) * std::size_t(w) + std::size_t(x0 + x)) * 4];
                    const float a = src[3] / 255.0f;
                    for (int ch = 0; ch < 3; ++ch)
                        dst[ch] = std::uint8_t(std::lround(src[ch] * a + dst[ch] * (1.0f - a)));
                }
            x0 += s * k + 16;
        }
        stbi_write_png(argv[2], w, h, 4, sheet.data(), w * 4);
        std::printf("wrote preview %s\n", argv[2]);
    }
    return 0;
}
