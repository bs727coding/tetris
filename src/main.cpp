// Claude Tetris - entry point: window, main loop, global hotkeys and --autotest.
#include "app.hpp"
#include "config.hpp"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

extern "C" void __stdcall glFinish(void);  // from opengl32; only used by --gputime

namespace {

bool g_gpuTiming = false;

void setupWindow(bool alwaysRun)
{
    // raylib normally sleeps while the window is minimized (saves battery); automated test
    // runs keep going instead so a minimized test window cannot stall them.
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIDDEN |
                   (alwaysRun ? FLAG_WINDOW_ALWAYS_RUN : 0u));
    InitWindow(1280, 720, "Tetris");
    const int monitor = GetCurrentMonitor();
    const int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    int h = int(float(mh) * 0.8f), w = h * 16 / 9;
    if (w > int(float(mw) * 0.92f)) {
        w = int(float(mw) * 0.92f);
        h = w * 9 / 16;
    }
    SetWindowSize(w, h);
    const Vector2 mp = GetMonitorPosition(monitor);
    SetWindowPosition(int(mp.x) + (mw - w) / 2, int(mp.y) + (mh - h) / 2);
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);

    // Window icon: a small purple T
    Image icon = GenImageColor(64, 64, BLANK);
    const Color body{ 186, 80, 255, 255 }, shine{ 226, 176, 255, 255 };
    auto cell = [&](int cx, int cy) {
        const int x = 8 + cx * 16, y = 16 + cy * 16;
        ImageDrawRectangle(&icon, x + 1, y + 1, 14, 14, body);
        ImageDrawRectangle(&icon, x + 1, y + 1, 14, 3, shine);
    };
    cell(1, 0);
    cell(0, 1);
    cell(1, 1);
    cell(2, 1);
    SetWindowIcon(icon);
    UnloadImage(icon);
}

void toggleFullscreen(App& app)
{
    ToggleBorderlessWindowed();
    app.book.prefs.fullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    app.book.savePrefs();
}

void handleHotkeys(App& app)
{
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (IsKeyPressed(KEY_F11) || (alt && IsKeyPressed(KEY_ENTER)))
        toggleFullscreen(app);
    if (IsKeyPressed(KEY_F3))
        app.showFps = !app.showFps;
    if (app.enteringName)
        return;  // letters belong to the name field
    Prefs& p = app.book.prefs;
    if (IsKeyPressed(KEY_M)) {
        p.music = !p.music;
        app.audio.setMusicEnabled(p.music);
        showToast(app, p.music ? "MUSIC ON" : "MUSIC OFF");
        app.book.savePrefs();
    }
    if (IsKeyPressed(KEY_N)) {
        p.sfx = !p.sfx;
        app.audio.setSfxEnabled(p.sfx);
        showToast(app, p.sfx ? "SOUND EFFECTS ON" : "SOUND EFFECTS OFF");
        app.book.savePrefs();
    }
    if (IsKeyPressed(KEY_B)) {
        p.bloom = !p.bloom;
        showToast(app, p.bloom ? "GLOW ON" : "GLOW OFF");
        app.book.savePrefs();
    }
}

// Scripted run used to verify the game end to end: plays through every major
// screen, saves screenshots and writes a frame-time report, then exits.
class AutoTest {
public:
    explicit AutoTest(std::filesystem::path dir) : dir_(std::move(dir)) {}

    void update(App& app, float dt)
    {
        t_ += dt;
        total_ += dt;
        if (total_ > 150.0f && step_ < 99) {
            finish(app, "TIMEOUT");
            return;
        }
        switch (step_) {
            case 0: if (t_ > 2.8f) shot("01_title.png"); break;
            case 1: goTo(app, Screen::ModeSelect); next(); break;
            case 2: if (app.screen == Screen::ModeSelect && app.screenTime > 1.0f) shot("02_modes.png"); break;
            case 3:
                app.book.prefs.startLevel = 6;
                startGame(app, GameMode::Marathon);
                next();
                break;
            case 4:  // photograph the first multi-line clear while its effects are on screen
                if (app.screen == Screen::Play && app.lastClearLines >= 2 && app.time - app.lastClearTime > 0.08f)
                    shot("03_clear.png");
                else if (t_ > 60.0f)
                    shot("03_clear.png");
                break;
            case 5: if (t_ > 6.0f) shot("04_play.png"); break;
            case 6: pauseGame(app); next(); break;
            case 7: if (t_ > 0.6f) shot("05_pause.png"); break;
            case 8:
                resumeGame(app);
                app.game->forceTopOut();
                next();
                break;
            case 9: if (app.screen == Screen::Results && app.screenTime > 1.2f) shot("06_results.png"); break;
            case 10:
                if (app.enteringName) {
                    app.name = "AUTOTEST";
                    submitName(app);
                } else {
                    app.lbMode = int(app.mode);
                    goTo(app, Screen::Leaderboard);
                }
                next();
                break;
            case 11: if (app.screen == Screen::Leaderboard && app.screenTime > 1.0f) shot("07_leaderboard.png"); break;
            case 12:
                app.controlsBack = Screen::Title;
                goTo(app, Screen::Controls);
                next();
                break;
            case 13: if (app.screen == Screen::Controls && app.screenTime > 0.8f) shot("08_controls.png"); break;
            case 14: finish(app, "OK"); break;
            default: break;
        }
    }

    // Called with the finished frame still in the back buffer
    void capture(const App& app, double cpuMs)
    {
        cpuMs_.push_back(cpuMs);
        frameMs_.push_back(double(GetFrameTime()) * 1000.0);
        if (GetTime() - lastTrace_ >= 1.0) {  // once a second: where are we, how fast are frames
            lastTrace_ = GetTime();
            std::ofstream trace(dir_ / "trace.txt", std::ios::app);
            trace << "t=" << GetTime() << " step=" << step_ << " screen=" << int(app.screen) << " phase=" << int(app.phase)
                  << " fps=" << GetFPS() << " frameMs=" << GetFrameTime() * 1000.0f << " cpuMs=" << cpuMs
                  << " pieces=" << (app.game ? app.game->stats().pieces : -1) << "\n";
        }
        const bool playing = app.screen == Screen::Play && app.phase == Phase::Live && !app.leaving;
        if (playing && wasPlaying_)  // steady gameplay frames only (no transitions or screenshots)
            liveMs_.push_back(double(GetFrameTime()) * 1000.0);
        wasPlaying_ = playing && pending_.empty();
        if (pending_.empty())
            return;
        Image img = LoadImageFromScreen();
        ExportImage(img, (dir_ / pending_).string().c_str());
        UnloadImage(img);
        shots_.push_back(pending_);
        pending_.clear();
    }

private:
    void next()
    {
        ++step_;
        t_ = 0.0f;
    }

    void shot(const char* name)
    {
        pending_ = name;
        next();
    }

    static double percentile(std::vector<double> v, double p)
    {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, std::size_t(p * double(v.size() - 1)))];
    }

    void finish(App& app, const char* status)
    {
        step_ = 99;
        app.quit = true;
        std::vector<double> cpu(cpuMs_.begin() + std::min<std::ptrdiff_t>(60, std::ptrdiff_t(cpuMs_.size())), cpuMs_.end());
        std::vector<double> frame(frameMs_.begin() + std::min<std::ptrdiff_t>(60, std::ptrdiff_t(frameMs_.size())), frameMs_.end());
        double avgFrame = 0.0;
        for (double f : frame)
            avgFrame += f;
        avgFrame = frame.empty() ? 0.0 : avgFrame / double(frame.size());
        std::ofstream out(dir_ / "report.txt");
        out << "status: " << status << "\n"
            << "frames: " << cpuMs_.size() << "\n"
            << "window: " << GetScreenWidth() << "x" << GetScreenHeight() << "\n"
            << "cpu ms per frame  p50 " << percentile(cpu, 0.5) << "  p95 " << percentile(cpu, 0.95) << "  p99 "
            << percentile(cpu, 0.99) << "  max " << percentile(cpu, 1.0) << "\n"
            << "frame interval ms avg " << avgFrame << "  (" << (avgFrame > 0 ? 1000.0 / avgFrame : 0.0) << " fps)"
            << "  p99 " << percentile(frame, 0.99) << "\n"
            << "gameplay frames: " << liveMs_.size() << "  interval ms p50 " << percentile(liveMs_, 0.5) << "  p99 "
            << percentile(liveMs_, 0.99) << "  max " << percentile(liveMs_, 1.0) << "  over 20 ms: "
            << std::count_if(liveMs_.begin(), liveMs_.end(), [](double v) { return v > 20.0; }) << "\n"
            << "screenshots:";
        for (const auto& s : shots_)
            out << " " << s;
        out << "\n" << app.audio.levelReport();
    }

    std::filesystem::path dir_;
    int step_ = 0;
    float t_ = 0.0f, total_ = 0.0f;
    std::string pending_;
    std::vector<std::string> shots_;
    std::vector<double> cpuMs_, frameMs_, liveMs_;
    bool wasPlaying_ = false;
    double lastTrace_ = 0.0;
};

void renderFrame(App& app, AutoTest* test, double frameStart)
{
    const Vector2 shake = app.fx.shakeOffset();
    const bool bloom = app.book.prefs.bloom && app.r.bloomSupported() && cfg::kBloomStrength > 0.0f;
    if (bloom) {
        app.r.beginGlow(shake);
        drawApp(app, Pass::Glow);
        app.r.endGlow();
    }
    app.r.renderBackdrop(app.time, app.hue, app.danger);
    BeginDrawing();
    ClearBackground(BLACK);
    app.r.drawBackdrop(app.time, app.hue);
    BeginMode2D(app.r.camera(shake));
    drawApp(app, Pass::Main);
    EndMode2D();
    if (bloom)
        app.r.compositeGlow(cfg::kBloomStrength);
    drawOverlay(app);
    if (g_gpuTiming) {  // wait for the GPU so the timing covers rendering, not just command submission
        rlDrawRenderBatchActive();
        glFinish();
    }
    app.cpuMs = (GetTime() - frameStart) * 1000.0;
    if (test)
        test->capture(app, app.cpuMs);
    EndDrawing();
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path autotestDir;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--autotest" && i + 1 < argc)
            autotestDir = argv[++i];
        else if (arg == "--gputime")  // developer flag: frame timings include the GPU's work
            g_gpuTiming = true;
    }

#ifdef NDEBUG
    SetTraceLogLevel(LOG_WARNING);
#endif
    setupWindow(!autotestDir.empty());

    auto app = std::make_unique<App>();
    std::unique_ptr<AutoTest> test;
    if (!autotestDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(autotestDir, ec);
        app->autotest = true;
        test = std::make_unique<AutoTest>(autotestDir);
    }
    app->book.load(app->autotest ? autotestDir : defaultDataDir());
    app->r.init();
    app->audio.init();
    app->audio.setMusicEnabled(app->book.prefs.music);
    app->audio.setSfxEnabled(app->book.prefs.sfx);

    ClearWindowState(FLAG_WINDOW_HIDDEN);
    if (app->book.prefs.fullscreen && !app->autotest)
        ToggleBorderlessWindowed();
    bootApp(*app);

    while (!WindowShouldClose() && !app->quit) {
        const double frameStart = GetTime();
        const float dt = std::min(GetFrameTime(), 0.05f);
        handleHotkeys(*app);
        app->r.beginFrame(dt);
        updateApp(*app, dt);
        if (test)
            test->update(*app, dt);
        renderFrame(*app, test.get(), frameStart);
    }

    app->book.savePrefs();
    app->audio.shutdown();
    app->r.shutdown();
    app.reset();
    CloseWindow();
    return 0;
}
