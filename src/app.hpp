#pragma once
// Application state shared by the screens, plus small UI helpers.
#include "ai.hpp"
#include "audio.hpp"
#include "fx.hpp"
#include "game.hpp"
#include "input.hpp"
#include "render.hpp"
#include "scores.hpp"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class Screen { Title, ModeSelect, Play, Results, Leaderboard, Controls, Credits };
enum class Phase { Countdown, Live, Paused, Ending };

// Easter eggs, remembered in Prefs::eggs as bits
inline constexpr int kEggKonami   = 1 << 0;  // up up down down left right left right B A on the title screen
inline constexpr int kEggClaude   = 1 << 1;  // type CLAUDE on the title screen
inline constexpr int kEggTune     = 1 << 2;  // type TETRIS, or click the logo letters T-E-T-R-I-S in order
inline constexpr int kEggCredits  = 1 << 3;  // type CREDITS on the title screen
inline constexpr int kEggBirthday = 1 << 4;  // launch on June 6, the day Tetris was first released (1984)
inline constexpr int kEggIdle     = 1 << 5;  // leave the game paused for a while
inline constexpr int kEggChampion = 1 << 6;  // beat Marathon (also unlocks the credits theme)
inline constexpr int kEggCount    = 7;

// Gameplay layout on the 1280x720 virtual canvas
namespace layout {
inline constexpr float kCell  = 30.0f;
inline constexpr float kWellX = 490.0f;  // top-left of the visible field
inline constexpr float kWellY = 84.0f;
inline constexpr float kWellW = kCell * kBoardW;
inline constexpr float kWellH = kCell * kVisibleH;
inline constexpr float kSideW = 196.0f;
inline constexpr float kLeftX  = kWellX - 24.0f - kSideW;
inline constexpr float kRightX = kWellX + kWellW + 24.0f;
} // namespace layout

struct App {
    Renderer r;
    Fx fx;
    Audio audio;
    ScoreBook book;
    InputHandler input;

    float time = 0.0f;  // seconds since launch
    Screen screen = Screen::Title;
    Screen pending = Screen::Title;
    float screenTime = 0.0f;
    float fade = 1.0f;  // 1 = fully black
    bool leaving = false;
    bool quit = false;
    float hue = 205.0f;   // backdrop hue, eased toward the level colour
    float danger = 0.0f;  // 0..1, how close the stack is to the top

    // menus
    int titleSel = 0;
    float titleHi = 0.0f;
    int modeSel = 0;
    float modeHi = 0.0f;
    float songNudge = 0.0f;  // slide of the song picker's title after a change (-1..1)
    int pauseSel = 0;
    float pauseHi = 0.0f;
    float pausedFor = 0.0f;
    int lbMode = 0;
    int lbHighlight = -1;
    Screen lbBack = Screen::Title;
    Screen controlsBack = Screen::Title;
    Vector2 lastMouse{ -1, -1 };

    // current run
    std::unique_ptr<Game> game;
    GameMode mode = GameMode::Marathon;
    int runSong = 0;  // the song playing under the current run
    Phase phase = Phase::Countdown;
    float phaseTime = 0.0f;
    double shownScore = 0.0;
    int countdownStep = -1;
    bool newBestShown = false;
    float softDropSfxTimer = 0.0f;
    float holdFlash = 0.0f;
    int greyRows = 0;
    int lastClearLines = 0;
    float lastClearTime = -10.0f;

    // results
    bool enteringName = false;
    std::string name;
    int newRank = -1;

    // attract mode on the title screen
    std::unique_ptr<Game> demo;
    AutoPlayer demoBot{ 7.0 };

    // end credits
    bool creditsWin = false;     // rolled by beating the game (shows the run, then the results)
    bool creditsUnlock = false;  // first win: announce the unlocked song
    float creditsScroll = 0.0f;
    float creditsFirework = 0.0f;

    // easter eggs
    int konami = 0;           // progress through the Konami code
    std::string typed;        // recent letters typed on the title screen
    bool party = false;       // Konami code: rainbow backdrop, confetti on every clear
    float logoDrop[6]{};      // screen time a logo letter was knocked (0 = never)
    int logoNext = 0;         // next letter of T-E-T-R-I-S to click
    float tuneTime = -1.0f;   // >= 0 while the logo plays the Korobeiniki opening
    bool birthdayDone = false;

    // HUD extras
    std::string toast;
    float toastTime = 0.0f;
    bool showFps = false;
    double cpuMs = 0.0;

    // --autotest
    bool autotest = false;
    AutoPlayer testBot{ 30.0 };
};

// screens.cpp ---------------------------------------------------------------
void bootApp(App& app);
void goTo(App& app, Screen screen);
void updateApp(App& app, float dt);
void drawApp(App& app, Pass pass);
void drawOverlay(App& app);  // screen space, after bloom
void showToast(App& app, const std::string& text);
void submitName(App& app);
void foundEgg(App& app, int egg);  // remembers a discovered easter egg
std::vector<int> songChoices(const App& app);  // pickable songs (locked ones left out), then -1 = shuffle

// play.cpp ------------------------------------------------------------------
void startGame(App& app, GameMode mode);
void enterPlay(App& app);
void updatePlay(App& app, float dt);
void drawPlay(App& app, Pass pass);
void pauseGame(App& app);
void resumeGame(App& app);

struct BoardView {
    Vector2 origin{ 0, 0 };  // top-left of the visible field
    float cell = 30.0f;
    float alpha = 1.0f;
    bool useFx = false;      // row-collapse animation offsets
    bool hideBlocks = false;
    int greyRows = 0;
    Color accent{ 0, 229, 255, 255 };
    float danger = 0.0f;
};
void drawBoard(App& app, const Game& game, const BoardView& view, Pass pass);
void drawPieceIcon(App& app, PieceType type, Vector2 center, float cell, float alpha, Pass pass, bool dim);

// ui helpers (screens.cpp) --------------------------------------------------
inline constexpr Color kTextBright{ 236, 240, 255, 255 };
inline constexpr Color kTextMuted{ 150, 160, 196, 255 };
inline constexpr Color kTextDim{ 100, 110, 146, 255 };
inline constexpr Color kGold{ 255, 206, 84, 255 };

Color modeColor(GameMode mode);
const char* modeName(GameMode mode);
float levelHue(int level);
Color hueColor(float hue, float sat = 0.62f, float val = 1.0f);
Color fadeColor(Color c, float alpha);
float roundness(Rectangle r, float radius);
void drawPanel(Rectangle r, Color accent, float alpha, Pass pass, float radius = 14.0f);
void drawLabel(App& app, const std::string& text, Vector2 pos, Color color, Align align = Align::Left, float size = 15.0f);
float keycapWidth(App& app, const std::string& key, float height);
float drawKeycap(App& app, const std::string& key, Vector2 pos, float height, Pass pass);  // returns width
void drawHints(App& app, std::initializer_list<std::pair<const char*, const char*>> hints, float y, Pass pass);
bool confirmPressed();
bool backPressed();
bool navPressed(int key);  // pressed or auto-repeating
Vector2 mouseVirtual(const App& app);
bool mouseMoved(App& app);
