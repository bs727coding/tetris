// Menus and the other non-gameplay screens, screen transitions and UI helpers.
#include "app.hpp"
#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>

using namespace layout;

namespace {

float easeOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float easeOutBounce(float x)
{
    const float n1 = 7.5625f, d1 = 2.75f;
    if (x < 1.0f / d1)
        return n1 * x * x;
    if (x < 2.0f / d1) {
        x -= 1.5f / d1;
        return n1 * x * x + 0.75f;
    }
    if (x < 2.5f / d1) {
        x -= 2.25f / d1;
        return n1 * x * x + 0.9375f;
    }
    x -= 2.625f / d1;
    return n1 * x * x + 0.984375f;
}

float lerpAngle(float a, float b, float t)
{
    const float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return std::fmod(a + d * t + 360.0f, 360.0f);
}

float stackDanger(const Game& g)
{
    return std::clamp(float(g.stackHeight() - 13) / 5.0f, 0.0f, 1.0f);
}

std::vector<std::string> splitWords(const char* s)
{
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

// Vertical menu shared by the title screen
void drawMenuHighlight(Rectangle hi, Color accent, Pass pass)
{
    const float rn = roundness(hi, 12.0f);
    if (pass == Pass::Main) {
        DrawRectangleRounded(hi, rn, 8, fadeColor(accent, 0.15f));
        DrawRectangleRoundedLinesEx(hi, rn, 8, 1.5f, fadeColor(accent, 0.85f));
    } else {
        DrawRectangleRoundedLinesEx(hi, rn, 8, 3.0f, fadeColor(accent, 0.6f));
    }
}

void onEnter(App& app, Screen s)
{
    app.screen = s;
    app.screenTime = 0.0f;
    switch (s) {
        case Screen::Play:
            enterPlay(app);
            break;
        case Screen::Results:
            app.fx.reset();
            app.audio.setSong(Song::Title);
            app.audio.setTempo(1.0f);
            app.audio.setMusicDuck(1.0f);
            if (app.newRank >= 0)
                app.audio.play(Sfx::NewRecord);
            break;
        default:
            app.audio.setSong(Song::Title);
            app.audio.setTempo(1.0f);
            app.audio.setMusicDuck(1.0f);
            break;
    }
}

// ---------------------------------------------------------------------------
//  Title
// ---------------------------------------------------------------------------
constexpr const char* kTitleItems[] = { "PLAY", "LEADERBOARDS", "CONTROLS", "QUIT" };
constexpr int kTitleCount = 4;

constexpr const char* kLogo[6][5] = {
    { "###", ".#.", ".#.", ".#.", ".#." },  // T
    { "###", "#..", "###", "#..", "###" },  // E
    { "###", ".#.", ".#.", ".#.", ".#." },  // T
    { "##.", "#.#", "##.", "#.#", "#.#" },  // R
    { "###", ".#.", ".#.", ".#.", "###" },  // I
    { "###", "#..", "###", "..#", "###" },  // S
};
constexpr int kLogoColor[6] = { 4, 6, 1, 3, 0, 2 };

Rectangle titleItemRect(float i)
{
    return { 640.0f - 160.0f, 300.0f + i * 62.0f, 320.0f, 50.0f };
}

void activateTitle(App& app, int item)
{
    app.audio.play(Sfx::MenuSelect);
    switch (item) {
        case 0:
            app.modeSel = app.book.prefs.lastMode;
            app.modeHi = float(app.modeSel);
            goTo(app, Screen::ModeSelect);
            break;
        case 1:
            app.lbMode = app.book.prefs.lastMode;
            app.lbHighlight = -1;
            app.lbBack = Screen::Title;
            goTo(app, Screen::Leaderboard);
            break;
        case 2:
            app.controlsBack = Screen::Title;
            goTo(app, Screen::Controls);
            break;
        default:
            app.quit = true;
            break;
    }
}

void updateDemo(App& app, float dt)
{
    if (!app.demo || app.demo->over()) {
        const auto seed = std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
        app.demo = std::make_unique<Game>(GameMode::Marathon, 4, app.autotest ? 99 : seed);
        app.demoBot = AutoPlayer(7.0);
    }
    app.demo->update(dt, app.demoBot.step(*app.demo, dt));
    app.demo->events().clear();
}

void updateTitle(App& app, float dt)
{
    app.titleHi += (float(app.titleSel) - app.titleHi) * std::min(1.0f, dt * 16.0f);
    if (navPressed(KEY_UP)) {
        app.titleSel = (app.titleSel + kTitleCount - 1) % kTitleCount;
        app.audio.play(Sfx::MenuMove);
    }
    if (navPressed(KEY_DOWN)) {
        app.titleSel = (app.titleSel + 1) % kTitleCount;
        app.audio.play(Sfx::MenuMove);
    }
    const Vector2 m = mouseVirtual(app);
    if (mouseMoved(app))
        for (int i = 0; i < kTitleCount; ++i)
            if (CheckCollisionPointRec(m, titleItemRect(float(i))) && app.titleSel != i) {
                app.titleSel = i;
                app.audio.play(Sfx::MenuMove);
            }
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, titleItemRect(float(app.titleSel)));
    if (confirmPressed() || click)
        activateTitle(app, app.titleSel);
    else if (IsKeyPressed(KEY_ESCAPE) && app.titleSel != kTitleCount - 1) {
        app.titleSel = kTitleCount - 1;
        app.audio.play(Sfx::MenuMove);
    }
}

void drawLogo(App& app, Pass pass)
{
    const float t = app.screenTime;
    constexpr float kBlock = 22.0f;
    const float x0 = 640.0f - 23.0f * kBlock * 0.5f, y0 = 58.0f;
    for (int letter = 0; letter < 6; ++letter)
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col) {
                if (kLogo[letter][row][col] != '#')
                    continue;
                const float delay = float(letter) * 0.09f + float(4 - row) * 0.035f;
                const float k = std::clamp((t - delay) / 0.6f, 0.0f, 1.0f);
                if (k <= 0.0f)
                    continue;
                const float drop = (1.0f - easeOutBounce(k)) * -460.0f;
                const float bob = std::sin(app.time * 2.2f + float(letter) * 0.7f) * 3.0f * std::clamp(t - 1.2f, 0.0f, 1.0f);
                const Rectangle rc{ x0 + float(letter * 4 + col) * kBlock, y0 + float(row) * kBlock + drop + bob, kBlock, kBlock };
                if (pass == Pass::Main)
                    app.r.tile(kLogoColor[letter], rc);
                else
                    app.r.tile(kTileWhite, rc, fadeColor(Renderer::pieceColor(kLogoColor[letter]), 0.55f));
            }
    const float sub = std::clamp((t - 0.9f) / 0.5f, 0.0f, 1.0f);
    if (pass == Pass::Main)
        app.r.text(Face::Display, "CLAUDE EDITION", { 640.0f, y0 + 5.0f * kBlock + 20.0f }, 20, fadeColor(kTextMuted, sub),
                   Align::Center, 14.0f);
}

void drawTopScores(App& app, Pass pass, Color accent)
{
    const Rectangle box{ 968.0f, 176.0f, 220.0f, 300.0f };
    drawPanel(box, accent, 1.0f, pass);
    if (pass != Pass::Main)
        return;
    drawLabel(app, "TOP SCORES", { box.x + 18.0f, box.y + 16.0f }, kTextMuted);
    drawLabel(app, "MARATHON", { box.x + box.width - 18.0f, box.y + 16.0f }, modeColor(GameMode::Marathon), Align::Right, 13.0f);
    const auto& list = app.book.entries(GameMode::Marathon);
    if (list.empty()) {
        app.r.text(Face::Ui, "No scores yet.", { box.x + box.width * 0.5f, box.y + 120.0f }, 17, kTextMuted, Align::Center);
        app.r.text(Face::Ui, "Play a round!", { box.x + box.width * 0.5f, box.y + 146.0f }, 17, kTextDim, Align::Center);
        return;
    }
    for (int i = 0; i < std::min<int>(5, int(list.size())); ++i) {
        const float y = box.y + 54.0f + float(i) * 46.0f;
        const ScoreEntry& e = list[std::size_t(i)];
        app.r.text(Face::Bold, std::to_string(i + 1), { box.x + 18.0f, y }, 18, i == 0 ? kGold : kTextMuted);
        app.r.text(Face::Ui, e.name, { box.x + 44.0f, y }, 16, kTextBright);
        app.r.textDigits(Face::Display, withCommas(e.score), { box.x + box.width - 18.0f, y + 20.0f }, 17, kTextMuted, Align::Right);
    }
}

void drawTitle(App& app, Pass pass)
{
    const Color accent = hueColor(app.hue);
    drawLogo(app, pass);

    if (app.demo) {
        BoardView v;
        v.origin = { 92.0f, 176.0f };
        v.cell = 22.0f;
        v.alpha = 0.55f;
        v.accent = accent;
        drawBoard(app, *app.demo, v, pass);
        if (pass == Pass::Main)
            drawLabel(app, "AUTOPLAY DEMO", { 92.0f + 110.0f, 146.0f }, kTextDim, Align::Center, 13.0f);
    }
    drawTopScores(app, pass, accent);

    const float menuAlpha = std::clamp((app.screenTime - 0.5f) / 0.4f, 0.0f, 1.0f);
    if (menuAlpha > 0.0f) {
        drawMenuHighlight(titleItemRect(app.titleHi), fadeColor(accent, menuAlpha), pass);
        if (pass == Pass::Main)
            for (int i = 0; i < kTitleCount; ++i) {
                const Rectangle r = titleItemRect(float(i));
                app.r.text(Face::Bold, kTitleItems[i], { r.x + r.width * 0.5f, r.y + 10.0f }, 26,
                           fadeColor(i == app.titleSel ? kTextBright : kTextMuted, menuAlpha), Align::Center, 4.0f);
            }
    }
    drawHints(app, { { "UP DOWN", "Navigate" }, { "Enter", "Select" }, { "M", "Music" }, { "F11", "Fullscreen" } }, 668.0f, pass);
}

// ---------------------------------------------------------------------------
//  Mode select
// ---------------------------------------------------------------------------
struct ModeInfo {
    const char* lines[3];
    PieceType icon;
};
const ModeInfo kModeInfo[kGameModes] = {
    { { "Endless survival.", "The speed rises", "every 10 lines." }, PieceType::T },
    { { "Clear 40 lines", "as fast as", "you can." }, PieceType::I },
    { { "Score as much", "as you can in", "two minutes." }, PieceType::L },
    { { "Relaxed stacking.", "No game over,", "no pressure." }, PieceType::O },
};

Rectangle modeCard(int i)
{
    return { 146.0f + float(i) * 252.0f, 176.0f, 232.0f, 350.0f };
}

void changeStartLevel(App& app, int delta)
{
    const int lv = std::clamp(app.book.prefs.startLevel + delta, 1, cfg::kMaxStartLevel);
    if (lv != app.book.prefs.startLevel) {
        app.book.prefs.startLevel = lv;
        app.audio.play(Sfx::MenuMove, 1.0f + 0.03f * float(lv));
    }
}

void updateModes(App& app, float dt)
{
    app.modeHi += (float(app.modeSel) - app.modeHi) * std::min(1.0f, dt * 14.0f);
    if (navPressed(KEY_LEFT)) {
        app.modeSel = (app.modeSel + kGameModes - 1) % kGameModes;
        app.audio.play(Sfx::MenuMove);
    }
    if (navPressed(KEY_RIGHT)) {
        app.modeSel = (app.modeSel + 1) % kGameModes;
        app.audio.play(Sfx::MenuMove);
    }
    if (app.modeSel == int(GameMode::Marathon)) {
        if (navPressed(KEY_UP))
            changeStartLevel(app, 1);
        if (navPressed(KEY_DOWN))
            changeStartLevel(app, -1);
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
            changeStartLevel(app, wheel > 0.0f ? 1 : -1);
    }
    const Vector2 m = mouseVirtual(app);
    if (mouseMoved(app))
        for (int i = 0; i < kGameModes; ++i)
            if (CheckCollisionPointRec(m, modeCard(i)) && app.modeSel != i) {
                app.modeSel = i;
                app.audio.play(Sfx::MenuMove);
            }
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, modeCard(app.modeSel));
    if (confirmPressed() || click) {
        app.audio.play(Sfx::MenuSelect);
        startGame(app, GameMode(app.modeSel));
    } else if (backPressed()) {
        app.audio.play(Sfx::MenuBack);
        goTo(app, Screen::Title);
    }
}

void drawModes(App& app, Pass pass)
{
    const Color accent = hueColor(app.hue);
    app.r.text(Face::Display, "SELECT MODE", { 640.0f, 62.0f }, 44, pass == Pass::Main ? kTextBright : fadeColor(accent, 0.6f),
               Align::Center, 10.0f);
    for (int i = 0; i < kGameModes; ++i) {
        const GameMode mode = GameMode(i);
        const bool sel = i == app.modeSel;
        const float focus = std::max(0.0f, 1.0f - std::fabs(float(i) - app.modeHi));
        Rectangle card = modeCard(i);
        card.y -= 12.0f * focus;
        const Color mc = modeColor(mode);
        const float rn = roundness(card, 18.0f);
        const float cx = card.x + card.width * 0.5f;
        if (pass == Pass::Main) {
            DrawRectangleRounded(card, rn, 10, fadeColor({ 10, 12, 30, 255 }, 0.78f + 0.14f * focus));
            DrawRectangleRoundedLinesEx(card, rn, 10, 1.2f + 1.5f * focus, fadeColor(mc, 0.3f + 0.7f * focus));
        } else if (focus > 0.05f) {
            DrawRectangleRoundedLinesEx(card, rn, 10, 4.0f, fadeColor(mc, 0.7f * focus));
        }
        app.r.text(Face::Bold, modeName(mode), { cx, card.y + 22.0f }, 30,
                   pass == Pass::Main ? mc : fadeColor(mc, 0.6f * focus), Align::Center, 3.0f);
        drawPieceIcon(app, kModeInfo[i].icon, { cx, card.y + 112.0f }, 26.0f, 0.6f + 0.4f * focus, pass, false);
        if (pass != Pass::Main)
            continue;
        for (int l = 0; l < 3; ++l)
            app.r.text(Face::Ui, kModeInfo[i].lines[l], { cx, card.y + 166.0f + float(l) * 24.0f }, 17,
                       sel ? kTextBright : kTextMuted, Align::Center);
        const ScoreEntry* best = app.book.best(mode);
        std::string bestText = "---";
        if (best)
            bestText = mode == GameMode::Sprint ? clockTime(best->time, true) : withCommas(best->score);
        drawLabel(app, "BEST", { cx, card.y + 250.0f }, kTextDim, Align::Center, 13.0f);
        app.r.textDigits(Face::Display, bestText, { cx, card.y + 268.0f }, 24, kTextBright, Align::Center);
        if (mode == GameMode::Marathon) {
            const float y = card.y + 308.0f;
            drawLabel(app, "START LEVEL", { card.x + 22.0f, y + 6.0f }, kTextMuted, Align::Left, 13.0f);
            app.r.textDigits(Face::Display, std::to_string(app.book.prefs.startLevel), { card.x + card.width - 48.0f, y }, 26,
                             sel ? mc : kTextBright, Align::Center);
            if (sel) {
                const float ax = card.x + card.width - 22.0f;
                DrawTriangle({ ax, y + 1.0f }, { ax - 6.0f, y + 10.0f }, { ax + 6.0f, y + 10.0f }, kTextMuted);
                DrawTriangle({ ax, y + 27.0f }, { ax + 6.0f, y + 18.0f }, { ax - 6.0f, y + 18.0f }, kTextMuted);
            }
        }
    }
    drawHints(app, { { "LEFT RIGHT", "Mode" }, { "UP DOWN", "Start level" }, { "Enter", "Play" }, { "Esc", "Back" } }, 668.0f, pass);
}

// ---------------------------------------------------------------------------
//  Results
// ---------------------------------------------------------------------------
void updateResults(App& app)
{
    if (app.enteringName) {
        for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
            const bool ok = ch < 128 && (std::isalnum(ch) || ch == ' ' || ch == '-' || ch == '_' || ch == '.');
            if (ok && app.name.size() < 12)
                app.name += char(std::toupper(ch));
        }
        if (navPressed(KEY_BACKSPACE) && !app.name.empty())
            app.name.pop_back();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ESCAPE))
            submitName(app);
        return;
    }
    if (confirmPressed() || IsKeyPressed(KEY_R)) {
        app.audio.play(Sfx::MenuSelect);
        startGame(app, app.mode);
    } else if (IsKeyPressed(KEY_L)) {
        app.audio.play(Sfx::MenuSelect);
        app.lbMode = int(app.mode);
        app.lbHighlight = -1;
        app.lbBack = Screen::ModeSelect;
        goTo(app, Screen::Leaderboard);
    } else if (backPressed()) {
        app.audio.play(Sfx::MenuBack);
        goTo(app, Screen::ModeSelect);
    }
}

void drawResults(App& app, Pass pass)
{
    const Game& g = *app.game;
    const Stats& s = g.stats();
    const Color mc = modeColor(app.mode);
    const bool main = pass == Pass::Main;

    const float k = easeOutCubic(app.screenTime / 0.5f);
    const Rectangle panel{ 340.0f, 62.0f + (1.0f - k) * 720.0f, 600.0f, 600.0f };
    drawPanel(panel, mc, 1.0f, pass, 24.0f);
    if (main)
        DrawRectangleRounded(panel, roundness(panel, 24.0f), 10, fadeColor({ 8, 10, 26, 255 }, 0.55f));
    const float cx = panel.x + panel.width * 0.5f;
    float y = panel.y + 28.0f;

    const char* heading = "GAME OVER";
    Color headColor{ 255, 80, 110, 255 };
    if (!g.toppedOut()) {
        headColor = kGold;
        heading = app.mode == GameMode::Sprint ? "FINISH!" : app.mode == GameMode::Ultra ? "TIME UP!" : "SESSION COMPLETE";
    }
    app.r.text(Face::Bold, heading, { cx, y }, 46, main ? headColor : fadeColor(headColor, 0.7f), Align::Center, 4.0f);
    y += 64.0f;
    if (main)
        drawLabel(app, modeName(app.mode), { cx, y }, mc, Align::Center, 16.0f);
    y += 34.0f;

    std::string big, bigLabel = "SCORE";
    if (app.mode == GameMode::Sprint) {
        bigLabel = g.completed() ? "TIME" : "LINES  (INCOMPLETE)";
        big = g.completed() ? clockTime(s.time, true) : fmt("%d / %d", s.lines, cfg::kSprintLines);
    } else {
        big = withCommas(s.score);
    }
    if (main)
        drawLabel(app, bigLabel, { cx, y }, kTextMuted, Align::Center, 14.0f);
    app.r.textDigits(Face::Display, big, { cx, y + 16.0f }, 64, main ? kTextBright : fadeColor(mc, 0.45f), Align::Center);
    y += 104.0f;

    const std::pair<const char*, std::string> cells[9] = {
        { "LINES", std::to_string(s.lines) },
        { "LEVEL", std::to_string(s.level) },
        { "PIECES", std::to_string(s.pieces) },
        { "TIME", clockTime(s.time, true) },
        { "PIECES / SEC", fmt("%.2f", s.pps()) },
        { "TETRISES", std::to_string(s.tetrises) },
        { "T-SPINS", std::to_string(s.tspins) },
        { "MAX COMBO", std::to_string(std::max(0, s.maxCombo)) },
        { "ALL CLEARS", std::to_string(s.perfectClears) },
    };
    if (main)
        for (int i = 0; i < 9; ++i) {
            const float colX = panel.x + 100.0f + float(i % 3) * 200.0f;
            const float rowY = y + float(i / 3) * 62.0f;
            drawLabel(app, cells[i].first, { colX, rowY }, kTextMuted, Align::Center, 13.0f);
            app.r.textDigits(Face::Display, cells[i].second, { colX, rowY + 18.0f }, 26, kTextBright, Align::Center);
        }
    y += 3.0f * 62.0f + 14.0f;

    const float hintsY = panel.y + panel.height - 44.0f;
    if (app.enteringName) {
        const float pulse = 0.6f + 0.4f * std::sin(app.time * 5.0f);
        app.r.text(Face::Bold, fmt("NEW RECORD  #%d", app.newRank + 1), { cx, y }, 26, main ? kGold : fadeColor(kGold, 0.6f * pulse),
                   Align::Center, 3.0f);
        const Rectangle box{ cx - 170.0f, y + 40.0f, 340.0f, 50.0f };
        if (main) {
            DrawRectangleRounded(box, roundness(box, 10.0f), 8, { 18, 20, 44, 240 });
            DrawRectangleRoundedLinesEx(box, roundness(box, 10.0f), 8, 2.0f, fadeColor(kGold, pulse));
            const bool caret = std::fmod(app.time, 1.0f) < 0.55f;
            app.r.text(Face::Display, app.name + (caret ? "_" : " "), { cx, box.y + 9.0f }, 30, kTextBright, Align::Center, 3.0f);
        } else {
            DrawRectangleRoundedLinesEx(box, roundness(box, 10.0f), 8, 3.0f, fadeColor(kGold, 0.5f * pulse));
        }
        drawHints(app, { { "Enter", "Save name" }, { "Backspace", "Delete" } }, hintsY, pass);
    } else {
        if (main && app.mode == GameMode::Sprint && !g.completed())
            app.r.text(Face::Ui, "Clear all 40 lines to set a time.", { cx, y + 10.0f }, 17, kTextDim, Align::Center);
        else if (main && app.newRank < 0)
            app.r.text(Face::Ui, "Not quite a top-10 run. Try again!", { cx, y + 10.0f }, 17, kTextDim, Align::Center);
        drawHints(app, { { "Enter", "Play again" }, { "L", "Leaderboard" }, { "Esc", "Menu" } }, hintsY, pass);
    }
}

// ---------------------------------------------------------------------------
//  Leaderboards
// ---------------------------------------------------------------------------
Rectangle lbTab(int i)
{
    return { 232.0f + float(i) * 208.0f, 100.0f, 192.0f, 44.0f };
}

void updateLeaderboard(App& app)
{
    if (navPressed(KEY_LEFT)) {
        app.lbMode = (app.lbMode + kGameModes - 1) % kGameModes;
        app.audio.play(Sfx::MenuMove);
    }
    if (navPressed(KEY_RIGHT)) {
        app.lbMode = (app.lbMode + 1) % kGameModes;
        app.audio.play(Sfx::MenuMove);
    }
    mouseMoved(app);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < kGameModes; ++i)
            if (CheckCollisionPointRec(mouseVirtual(app), lbTab(i)) && app.lbMode != i) {
                app.lbMode = i;
                app.audio.play(Sfx::MenuMove);
            }
    if (confirmPressed() || backPressed()) {
        app.audio.play(Sfx::MenuBack);
        goTo(app, app.lbBack);
    }
}

void drawLeaderboard(App& app, Pass pass)
{
    const bool main = pass == Pass::Main;
    const Color accent = hueColor(app.hue);
    app.r.text(Face::Display, "LEADERBOARDS", { 640.0f, 30.0f }, 42, main ? kTextBright : fadeColor(accent, 0.6f), Align::Center, 10.0f);

    for (int i = 0; i < kGameModes; ++i) {
        const Rectangle tab = lbTab(i);
        const bool sel = i == app.lbMode;
        const Color mc = modeColor(GameMode(i));
        const float rn = roundness(tab, 10.0f);
        if (main) {
            if (sel)
                DrawRectangleRounded(tab, rn, 8, fadeColor(mc, 0.16f));
            DrawRectangleRoundedLinesEx(tab, rn, 8, sel ? 2.0f : 1.0f, fadeColor(mc, sel ? 0.95f : 0.3f));
        } else if (sel) {
            DrawRectangleRoundedLinesEx(tab, rn, 8, 3.0f, fadeColor(mc, 0.6f));
        }
        if (main)
            app.r.text(Face::Bold, modeName(GameMode(i)), { tab.x + tab.width * 0.5f, tab.y + 10.0f }, 20, sel ? mc : kTextMuted,
                       Align::Center, 3.0f);
    }

    const GameMode mode = GameMode(app.lbMode);
    const Color mc = modeColor(mode);
    const Rectangle table{ 200.0f, 164.0f, 880.0f, 476.0f };
    drawPanel(table, mc, 1.0f, pass, 18.0f);
    if (!main)
        return;

    const bool sprint = mode == GameMode::Sprint;
    const float hy = table.y + 16.0f;
    drawLabel(app, "#", { 240.0f, hy }, kTextDim, Align::Center, 13.0f);
    drawLabel(app, "NAME", { 276.0f, hy }, kTextDim, Align::Left, 13.0f);
    drawLabel(app, sprint ? "TIME" : "SCORE", { 640.0f, hy }, kTextDim, Align::Right, 13.0f);
    drawLabel(app, "LINES", { 740.0f, hy }, kTextDim, Align::Right, 13.0f);
    drawLabel(app, "LEVEL", { 830.0f, hy }, kTextDim, Align::Right, 13.0f);
    drawLabel(app, sprint ? "SCORE" : "TIME", { 950.0f, hy }, kTextDim, Align::Right, 13.0f);
    drawLabel(app, "DATE", { 1056.0f, hy }, kTextDim, Align::Right, 13.0f);

    const auto& list = app.book.entries(mode);
    const Color medals[3] = { kGold, { 205, 215, 235, 255 }, { 220, 145, 85, 255 } };
    for (int i = 0; i < kLeaderboardSize; ++i) {
        const float y = table.y + 48.0f + float(i) * 42.0f;
        const Rectangle row{ table.x + 12.0f, y - 6.0f, table.width - 24.0f, 38.0f };
        const bool highlight = i == app.lbHighlight && mode == app.mode;
        if (highlight) {
            const float pulse = 0.5f + 0.5f * std::sin(app.time * 4.0f);
            DrawRectangleRounded(row, roundness(row, 8.0f), 6, fadeColor(mc, 0.14f + 0.12f * pulse));
            DrawRectangleRoundedLinesEx(row, roundness(row, 8.0f), 6, 1.5f, fadeColor(mc, 0.9f));
        } else if (i % 2 == 0) {
            DrawRectangleRounded(row, roundness(row, 8.0f), 6, fadeColor(WHITE, 0.03f));
        }
        const Color rankColor = i < 3 ? medals[i] : kTextMuted;
        app.r.text(Face::Bold, std::to_string(i + 1), { 240.0f, y }, 20, rankColor, Align::Center);
        if (i >= int(list.size())) {
            app.r.text(Face::Ui, "---", { 276.0f, y + 2.0f }, 17, kTextDim);
            continue;
        }
        const ScoreEntry& e = list[std::size_t(i)];
        app.r.text(Face::Ui, e.name, { 276.0f, y + 1.0f }, 18, kTextBright);
        app.r.textDigits(Face::Display, sprint ? clockTime(e.time, true) : withCommas(e.score), { 640.0f, y }, 22,
                         highlight ? mc : kTextBright, Align::Right);
        app.r.textDigits(Face::Display, std::to_string(e.lines), { 740.0f, y + 2.0f }, 19, kTextMuted, Align::Right);
        app.r.textDigits(Face::Display, std::to_string(e.level), { 830.0f, y + 2.0f }, 19, kTextMuted, Align::Right);
        app.r.textDigits(Face::Display, sprint ? withCommas(e.score) : clockTime(e.time, false), { 950.0f, y + 2.0f }, 19,
                         kTextMuted, Align::Right);
        app.r.textDigits(Face::Display, e.date, { 1056.0f, y + 2.0f }, 17, kTextDim, Align::Right);
    }
    drawHints(app, { { "LEFT RIGHT", "Mode" }, { "Esc", "Back" } }, 668.0f, pass);
}

// ---------------------------------------------------------------------------
//  Controls
// ---------------------------------------------------------------------------
struct Binding {
    const char* keys;
    const char* action;
};
constexpr Binding kPlayKeys[] = {
    { "LEFT RIGHT", "Move" },       { "DOWN", "Soft drop" },
    { "Space", "Hard drop" },       { "UP X", "Rotate clockwise" },
    { "Z Ctrl", "Rotate counter-clockwise" }, { "A", "Rotate 180" },
    { "C Shift", "Hold" },          { "Esc P", "Pause" },
};
constexpr Binding kSystemKeys[] = {
    { "M", "Music on / off" }, { "N", "Sound effects on / off" }, { "B", "Glow on / off" },
    { "F11", "Fullscreen" },   { "F3", "FPS counter" },
};

void drawBindings(App& app, Pass pass, const char* title, const Binding* b, int n, Rectangle box, float keysRight)
{
    drawPanel(box, hueColor(app.hue), 1.0f, pass, 18.0f);
    if (pass != Pass::Main)
        return;
    drawLabel(app, title, { box.x + 24.0f, box.y + 18.0f }, kTextMuted);
    for (int i = 0; i < n; ++i) {
        const float y = box.y + 56.0f + float(i) * 48.0f;
        const auto keys = splitWords(b[i].keys);
        constexpr float kH = 30.0f, kGap = 6.0f;
        float total = -kGap;
        for (const auto& k : keys)
            total += keycapWidth(app, k, kH) + kGap;
        float x = keysRight - total;
        for (const auto& k : keys)
            x += drawKeycap(app, k, { x, y }, kH, pass) + kGap;
        app.r.text(Face::Ui, b[i].action, { keysRight + 18.0f, y + 4.0f }, 18, kTextBright);
    }
}

void updateControls(App& app)
{
    if (confirmPressed() || backPressed()) {
        app.audio.play(Sfx::MenuBack);
        goTo(app, app.controlsBack);
    }
}

void drawControls(App& app, Pass pass)
{
    const Color accent = hueColor(app.hue);
    app.r.text(Face::Display, "CONTROLS", { 640.0f, 30.0f }, 42, pass == Pass::Main ? kTextBright : fadeColor(accent, 0.6f),
               Align::Center, 10.0f);
    drawBindings(app, pass, "GAMEPLAY", kPlayKeys, int(std::size(kPlayKeys)), { 130.0f, 110.0f, 520.0f, 450.0f }, 320.0f);
    drawBindings(app, pass, "SYSTEM", kSystemKeys, int(std::size(kSystemKeys)), { 670.0f, 110.0f, 480.0f, 450.0f }, 790.0f);
    if (pass == Pass::Main)
        app.r.text(Face::Ui, "Handling: DAS 133 ms, ARR 33 ms. Tune these and more in src/config.hpp.", { 640.0f, 590.0f }, 16,
                   kTextDim, Align::Center);
    drawHints(app, { { "Esc", "Back" } }, 668.0f, pass);
}

} // namespace

// ---------------------------------------------------------------------------
//  Screen management
// ---------------------------------------------------------------------------
void bootApp(App& app)
{
    app.modeSel = app.book.prefs.lastMode;
    app.modeHi = float(app.modeSel);
    app.fade = 1.0f;
    app.leaving = false;
    onEnter(app, Screen::Title);
}

void goTo(App& app, Screen screen)
{
    if (app.leaving)
        return;
    app.pending = screen;
    app.leaving = true;
}

void showToast(App& app, const std::string& text)
{
    app.toast = text;
    app.toastTime = 1.6f;
}

void submitName(App& app)
{
    std::string name = app.name;
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    if (name.empty())
        name = "PLAYER";
    const Stats& s = app.game->stats();
    ScoreEntry e;
    e.name = name;
    e.score = s.score;
    e.lines = s.lines;
    e.level = s.level;
    e.time = s.time;
    e.date = todayString();
    const int rank = app.book.insert(app.mode, e);
    app.book.prefs.lastName = name;
    app.book.savePrefs();
    app.enteringName = false;
    app.lbMode = int(app.mode);
    app.lbHighlight = rank;
    app.lbBack = Screen::ModeSelect;
    app.audio.play(Sfx::MenuSelect);
    goTo(app, Screen::Leaderboard);
}

void updateApp(App& app, float dt)
{
    app.time += dt;
    app.screenTime += dt;
    app.toastTime = std::max(0.0f, app.toastTime - dt);
    if (app.leaving) {
        app.fade = std::min(1.0f, app.fade + dt / 0.16f);
        if (app.fade >= 1.0f) {
            app.leaving = false;
            onEnter(app, app.pending);
        }
    } else {
        app.fade = std::max(0.0f, app.fade - dt / 0.22f);
    }

    float targetHue = std::fmod(215.0f + 25.0f * std::sin(app.time * 0.07f) + 360.0f, 360.0f);
    float targetDanger = 0.0f;
    if (app.game && (app.screen == Screen::Play || app.screen == Screen::Results)) {
        targetHue = levelHue(app.game->stats().level);
        if (app.screen == Screen::Play && app.phase != Phase::Paused && !app.game->over())
            targetDanger = stackDanger(*app.game);
    }
    app.hue = lerpAngle(app.hue, targetHue, 1.0f - std::exp(-dt * 2.5f));
    app.danger += (targetDanger - app.danger) * (1.0f - std::exp(-dt * 4.0f));

    app.fx.update(dt);
    if (app.screen == Screen::Title)
        updateDemo(app, dt);
    if (app.leaving)
        return;

    switch (app.screen) {
        case Screen::Title:       updateTitle(app, dt); break;
        case Screen::ModeSelect:  updateModes(app, dt); break;
        case Screen::Play:        updatePlay(app, dt); break;
        case Screen::Results:     updateResults(app); break;
        case Screen::Leaderboard: updateLeaderboard(app); break;
        case Screen::Controls:    updateControls(app); break;
    }
}

void drawApp(App& app, Pass pass)
{
    switch (app.screen) {
        case Screen::Title:       drawTitle(app, pass); break;
        case Screen::ModeSelect:  drawModes(app, pass); break;
        case Screen::Play:        drawPlay(app, pass); break;
        case Screen::Results:     drawResults(app, pass); break;
        case Screen::Leaderboard: drawLeaderboard(app, pass); break;
        case Screen::Controls:    drawControls(app, pass); break;
    }
}

void drawOverlay(App& app)
{
    app.fx.drawScreenFlash(app.r);
    BeginMode2D(app.r.camera());
    if (app.toastTime > 0.0f) {
        const float a = std::clamp(app.toastTime / 0.3f, 0.0f, 1.0f);
        const float w = app.r.measure(Face::Bold, app.toast, 18, 2.0f).x;
        const Rectangle pill{ 640.0f - w * 0.5f - 24.0f, 606.0f, w + 48.0f, 42.0f };
        DrawRectangleRounded(pill, 1.0f, 12, fadeColor({ 14, 16, 36, 255 }, 0.92f * a));
        DrawRectangleRoundedLinesEx(pill, 1.0f, 12, 1.5f, fadeColor(hueColor(app.hue), 0.8f * a));
        app.r.text(Face::Bold, app.toast, { 640.0f, 616.0f }, 18, fadeColor(kTextBright, a), Align::Center, 2.0f);
    }
    if (app.showFps)
        app.r.text(Face::Ui, fmt("%d FPS   %.2f ms", GetFPS(), app.cpuMs), { 12.0f, 8.0f }, 15, kTextMuted);
    EndMode2D();
    if (app.fade > 0.0f)
        DrawRectangle(0, 0, app.r.width(), app.r.height(), fadeColor(BLACK, app.fade));
}

// ---------------------------------------------------------------------------
//  UI helpers
// ---------------------------------------------------------------------------
Color modeColor(GameMode mode)
{
    switch (mode) {
        case GameMode::Marathon: return { 0, 229, 255, 255 };
        case GameMode::Sprint:   return { 60, 240, 110, 255 };
        case GameMode::Ultra:    return { 255, 146, 28, 255 };
        case GameMode::Zen:      return { 186, 80, 255, 255 };
    }
    return WHITE;
}

const char* modeName(GameMode mode)
{
    switch (mode) {
        case GameMode::Marathon: return "MARATHON";
        case GameMode::Sprint:   return "SPRINT";
        case GameMode::Ultra:    return "ULTRA";
        case GameMode::Zen:      return "ZEN";
    }
    return "";
}

float levelHue(int level)
{
    return std::fmod(195.0f + float(level - 1) * 31.0f, 360.0f);
}

Color hueColor(float hue, float sat, float val)
{
    return ColorFromHSV(hue, sat, val);
}

Color fadeColor(Color c, float alpha)
{
    c.a = (unsigned char)std::clamp(alpha * float(c.a), 0.0f, 255.0f);
    return c;
}

float roundness(Rectangle r, float radius)
{
    const float m = std::min(r.width, r.height);
    return m > 0.0f ? std::clamp(2.0f * radius / m, 0.0f, 1.0f) : 0.0f;
}

void drawPanel(Rectangle r, Color accent, float alpha, Pass pass, float radius)
{
    const float rn = roundness(r, radius);
    if (pass == Pass::Main) {
        DrawRectangleRounded(r, rn, 10, fadeColor({ 10, 12, 30, 255 }, 0.8f * alpha));
        DrawRectangleRoundedLinesEx(r, rn, 10, 1.5f, fadeColor(accent, 0.45f * alpha));
    } else {
        DrawRectangleRoundedLinesEx(r, rn, 10, 2.0f, fadeColor(accent, 0.22f * alpha));
    }
}

void drawLabel(App& app, const std::string& text, Vector2 pos, Color color, Align align, float size)
{
    app.r.text(Face::Ui, text, pos, size, color, align, size * 0.2f);
}

float keycapWidth(App& app, const std::string& key, float h)
{
    const bool arrow = key == "UP" || key == "DOWN" || key == "LEFT" || key == "RIGHT";
    return arrow ? h : std::max(h, app.r.measure(Face::Ui, key, h * 0.5f).x + h * 0.6f);
}

float drawKeycap(App& app, const std::string& key, Vector2 pos, float h, Pass pass)
{
    const float fontSize = h * 0.5f;
    const float w = keycapWidth(app, key, h);
    if (pass != Pass::Main)
        return w;
    const Rectangle r{ pos.x, pos.y, w, h };
    const float rn = roundness(r, 6.0f);
    DrawRectangleRounded({ r.x, r.y + 2.0f, r.width, r.height }, rn, 6, fadeColor(BLACK, 0.5f));
    DrawRectangleRounded(r, rn, 6, { 34, 38, 66, 235 });
    DrawRectangleRoundedLinesEx(r, rn, 6, 1.2f, { 120, 130, 176, 200 });
    const Vector2 c{ r.x + w * 0.5f, r.y + h * 0.5f };
    const float s = h * 0.2f;
    if (key == "UP")
        DrawTriangle({ c.x, c.y - s }, { c.x - s, c.y + s * 0.7f }, { c.x + s, c.y + s * 0.7f }, kTextBright);
    else if (key == "DOWN")
        DrawTriangle({ c.x, c.y + s }, { c.x + s, c.y - s * 0.7f }, { c.x - s, c.y - s * 0.7f }, kTextBright);
    else if (key == "LEFT")
        DrawTriangle({ c.x - s, c.y }, { c.x + s * 0.7f, c.y + s }, { c.x + s * 0.7f, c.y - s }, kTextBright);
    else if (key == "RIGHT")
        DrawTriangle({ c.x + s, c.y }, { c.x - s * 0.7f, c.y - s }, { c.x - s * 0.7f, c.y + s }, kTextBright);
    else
        app.r.text(Face::Ui, key, { c.x, c.y - fontSize * 0.62f }, fontSize, kTextBright, Align::Center);
    return w;
}

void drawHints(App& app, std::initializer_list<std::pair<const char*, const char*>> hints, float y, Pass pass)
{
    if (pass != Pass::Main)
        return;
    constexpr float kH = 26.0f, kKeyGap = 5.0f, kLabelGap = 9.0f, kHintGap = 28.0f, kSize = 15.0f;
    float total = -kHintGap;
    for (const auto& [keys, label] : hints) {
        for (const auto& k : splitWords(keys))
            total += keycapWidth(app, k, kH) + kKeyGap;
        total += kLabelGap - kKeyGap + app.r.measure(Face::Ui, label, kSize).x + kHintGap;
    }
    float x = 640.0f - total * 0.5f;
    for (const auto& [keys, label] : hints) {
        for (const auto& k : splitWords(keys))
            x += drawKeycap(app, k, { x, y }, kH, pass) + kKeyGap;
        x += kLabelGap - kKeyGap;
        app.r.text(Face::Ui, label, { x, y + kH * 0.5f - kSize * 0.62f }, kSize, kTextMuted);
        x += app.r.measure(Face::Ui, label, kSize).x + kHintGap;
    }
}

bool confirmPressed()
{
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    return !alt && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE));
}

bool backPressed()
{
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
}

bool navPressed(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

Vector2 mouseVirtual(const App& app)
{
    return app.r.toVirtual(GetMousePosition());
}

bool mouseMoved(App& app)
{
    const Vector2 m = GetMousePosition();
    const bool moved = app.lastMouse.x >= 0.0f && (m.x != app.lastMouse.x || m.y != app.lastMouse.y);
    app.lastMouse = m;
    return moved;
}
