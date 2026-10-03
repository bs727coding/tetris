#pragma once
// ---------------------------------------------------------------------------
//  Every tunable value in one place. Edit, then rebuild.
// ---------------------------------------------------------------------------

namespace cfg {

// --- Handling (seconds) ---------------------------------------------------
inline constexpr double kDas            = 0.133;  // hold time before auto-shift starts
inline constexpr double kArr            = 0.033;  // auto-shift repeat interval (0 = jump to wall)
inline constexpr double kSoftDropFactor = 20.0;   // soft drop speed = gravity x factor
inline constexpr double kLockDelay      = 0.5;    // time a grounded piece waits before locking
inline constexpr int    kMaxLockResets  = 15;     // moves/rotations that may refresh the lock delay

// --- Rules ----------------------------------------------------------------
inline constexpr int    kPreviewCount   = 5;      // pieces shown in the NEXT queue
inline constexpr int    kLinesPerLevel  = 10;
inline constexpr int    kMaxLevel       = 20;     // gravity stops getting faster here
inline constexpr int    kMaxStartLevel  = 15;
inline constexpr int    kSprintLines    = 40;
inline constexpr int    kMarathonLines  = 200;    // clearing this many beats the game (0 = endless)
inline constexpr double kUltraSeconds   = 120.0;
inline constexpr int    kZenLevel       = 1;      // Zen uses a fixed, gentle gravity

// --- Presentation ---------------------------------------------------------
inline constexpr float  kBloomStrength  = 1.0f;   // 0 = no glow at all
inline constexpr float  kShakeStrength  = 1.0f;   // 0 = no screen shake
inline constexpr bool   kSmoothFall     = true;   // glide pieces between rows

// --- Audio ----------------------------------------------------------------
inline constexpr float  kMusicVolume    = 0.45f;
inline constexpr float  kSfxVolume      = 0.70f;
inline constexpr float  kMusicBpm       = 140.0f; // game theme tempo at level 1

// --- Palette (neon) ---------------------------------------------------------
struct Rgb { unsigned char r, g, b; };
inline constexpr Rgb kPieceColors[7] = {
    {   0, 229, 255 },  // I  cyan
    { 255, 214,  10 },  // O  yellow
    { 186,  80, 255 },  // T  purple
    {  60, 240, 110 },  // S  green
    { 255,  56,  96 },  // Z  red
    {  50, 120, 255 },  // J  blue
    { 255, 146,  28 },  // L  orange
};
inline constexpr Rgb kGreyColor = { 118, 122, 150 };

} // namespace cfg
