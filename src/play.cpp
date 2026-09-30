// The gameplay screen: board, HUD, countdown, pause menu, game-over sequence,
// and the translation of engine events into sound and visual effects.
#include "app.hpp"
#include "config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace layout;

namespace {

constexpr float kCountStep = 0.55f;
constexpr Vector2 kWellCenter{ kWellX + kWellW * 0.5f, kWellY + kWellH * 0.5f };
constexpr const char* kClearNames[5] = { "", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS" };

Color gameAccent(const App& app)
{
    return hueColor(levelHue(app.game->stats().level));
}

Rectangle cellRect(int x, float y)
{
    return { kWellX + float(x) * kCell, kWellY + (y - float(kHiddenH)) * kCell, kCell, kCell };
}

float panFor(int x)
{
    return (float(x) - 3.5f) / 10.0f;
}

// ---------------------------------------------------------------------------
//  Events -> feedback
// ---------------------------------------------------------------------------
void enterEnding(App& app)
{
    if (app.phase == Phase::Ending)
        return;
    app.phase = Phase::Ending;
    app.phaseTime = 0.0f;
    app.greyRows = 0;
    app.audio.setSong(Song::None);
    const Game& g = *app.game;
    const Vector2 at{ kWellCenter.x, kWellCenter.y - 30.0f };
    if (g.toppedOut()) {
        app.fx.popup("GAME OVER", at, 62, { 255, 80, 110, 255 }, 2.2f, 0.35f, Face::Bold, 0, Slot::Center);
        app.fx.shake(0.55f);
    } else {
        const char* title = g.mode() == GameMode::Sprint ? "FINISH!" : g.mode() == GameMode::Ultra ? "TIME UP!" : "WELL PLAYED";
        app.fx.popup(title, at, 62, kGold, 2.4f, 0.1f, Face::Bold, 0, Slot::Center);
        app.fx.confetti({ kWellX - 30.0f, kWellY + kWellH }, 110, 950.0f, -62.0f, 36.0f);
        app.fx.confetti({ kWellX + kWellW + 30.0f, kWellY + kWellH }, 110, 950.0f, -118.0f, 36.0f);
        app.fx.flash(WHITE, 0.35f);
    }
}

void onLineClear(App& app, const GameEvent& e)
{
    const int n = e.lines;
    float rowsCenter = 0.0f;
    for (int i = 0; i < n; ++i) {
        const int y = e.rows[std::size_t(i)];
        const float top = kWellY + (float(y - kHiddenH) + app.fx.rowOffset(y)) * kCell;
        rowsCenter += top + kCell * 0.5f;
        app.fx.rowFlash({ kWellX, top, kWellW, kCell }, WHITE);
        for (int x = 0; x < kBoardW; ++x) {
            const int v = e.rowCells[std::size_t(i)][std::size_t(x)];
            const Color c = Renderer::pieceColor(v == kGreyCell ? 7 : v - 1);
            const Vector2 p{ kWellX + (float(x) + 0.5f) * kCell, top + kCell * 0.5f };
            app.fx.burst(p, c, n == 4 ? 7 : 4, n == 4 ? 560.0f : 400.0f, ParticleKind::Spark, 0.75f, 6.0f, 140.0f);
            app.fx.burst(p, c, 1, 280.0f, ParticleKind::Chunk, 0.9f, 9.0f, 240.0f);
        }
    }
    rowsCenter /= float(std::max(1, n));
    app.fx.collapseRows(e.rows, n);
    app.lastClearLines = n;
    app.lastClearTime = app.time;

    const bool spin = e.spin != Spin::None;
    std::string title = kClearNames[n];
    if (e.spin == Spin::Full)
        title = "T-SPIN " + title;
    else if (e.spin == Spin::Mini)
        title = "T-SPIN MINI " + title;
    const Color tint = spin ? Color{ 196, 110, 255, 255 } : n == 4 ? Color{ 60, 235, 255, 255 } : kTextBright;
    const float size = (n == 4 || e.spin == Spin::Full) ? 54.0f : 40.0f;
    const Vector2 mid{ kWellCenter.x, std::clamp(rowsCenter - 40.0f, kWellY + 150.0f, kWellY + kWellH - 120.0f) };
    app.fx.popup(title, mid, size, tint, 1.3f, 0.0f, Face::Bold, 36.0f, Slot::ClearName);
    if (e.b2b)
        app.fx.popup("BACK-TO-BACK", { mid.x, mid.y - size * 0.95f }, 22, kGold, 1.3f, 0.05f, Face::Ui, 36.0f, Slot::B2B);
    app.fx.popup("+" + withCommas(e.points), { mid.x, mid.y + size * 0.8f }, 24, kTextBright, 1.1f, 0.1f, Face::Display, 50.0f,
                 Slot::Points);
    if (e.combo >= 1)
        app.fx.popup(fmt("COMBO x%d", e.combo), { kLeftX + kSideW * 0.5f, kWellY + 470.0f }, 30, { 255, 150, 40, 255 },
                     1.0f, 0.0f, Face::Bold, 24.0f, Slot::Combo);

    if (spin)
        app.audio.play(Sfx::TSpinClear);
    else
        app.audio.play(n == 4 ? Sfx::Tetris : n == 3 ? Sfx::Clear3 : n == 2 ? Sfx::Clear2 : Sfx::Clear1);
    if (e.b2b)
        app.audio.play(Sfx::BackToBack, 1.0f, 0.8f);
    if (e.combo >= 1)
        app.audio.play(Sfx::Combo, std::exp2(float(std::min(e.combo, 12)) / 12.0f), 0.9f);

    const bool big = n == 4 || (e.spin == Spin::Full && n >= 2);
    app.fx.shake(big ? 0.5f : 0.1f * float(n));
    if (big) {
        app.fx.ring({ kWellCenter.x, rowsCenter }, 430.0f, tint, 0.7f);
        app.fx.flash(tint, 0.3f);
    }
    if (e.perfectClear) {
        app.audio.play(Sfx::AllClear);
        app.fx.popup("ALL CLEAR", { kWellCenter.x, kWellY + kWellH * 0.36f }, 62, kGold, 2.0f, 0.15f, Face::Bold, 20.0f);
        app.fx.confetti({ kWellX - 20.0f, kWellY + kWellH }, 90, 900.0f, -60.0f, 40.0f);
        app.fx.confetti({ kWellX + kWellW + 20.0f, kWellY + kWellH }, 90, 900.0f, -120.0f, 40.0f);
        app.fx.flash(WHITE, 0.5f);
    }
}

void handleEvents(App& app)
{
    Game& g = *app.game;
    auto& events = g.events();
    const bool anyClear = std::any_of(events.begin(), events.end(),
                                      [](const GameEvent& e) { return e.type == EventType::LineClear; });
    for (const GameEvent& e : events) {
        const Color pc = Renderer::pieceColor(int(e.piece));
        switch (e.type) {
            case EventType::Move:
                app.audio.play(Sfx::Move, 1.0f, 0.8f, panFor(g.active().x));
                break;
            case EventType::Rotate:
                app.audio.play(Sfx::Rotate, e.value > 0 ? 1.1f : 1.0f, 0.9f, panFor(g.active().x));
                break;
            case EventType::Hold:
                app.audio.play(Sfx::Hold);
                app.holdFlash = 1.0f;
                break;
            case EventType::SoftDrop:
                if (app.softDropSfxTimer <= 0.0f) {
                    app.audio.play(Sfx::SoftDrop, 1.0f, 0.6f);
                    app.softDropSfxTimer = 0.05f;
                }
                break;
            case EventType::HardDrop: {
                app.audio.play(Sfx::HardDrop, 1.0f, std::min(1.0f, 0.55f + 0.03f * float(e.value)));
                for (int x = 0; x < kBoardW; ++x) {  // a light trail per occupied column
                    int top = kBoardH, bottom = -1;
                    for (const Point c : e.cells)
                        if (c.x == x) {
                            top = std::min(top, c.y);
                            bottom = std::max(bottom, c.y);
                        }
                    if (bottom < 0 || e.value <= 0)
                        continue;
                    const float y0 = std::max(kWellY - 2.0f * kCell, kWellY + float(top - e.value - kHiddenH) * kCell);
                    const float y1 = kWellY + float(bottom + 1 - kHiddenH) * kCell;
                    app.fx.trail({ kWellX + float(x) * kCell + 3.0f, y0, kCell - 6.0f, y1 - y0 }, pc);
                }
                for (const Point c : e.cells) {  // dust puffs under the piece
                    const bool exposedBottom = std::none_of(e.cells.begin(), e.cells.end(),
                                                            [&](const Point o) { return o.x == c.x && o.y == c.y + 1; });
                    if (exposedBottom)
                        app.fx.burst({ kWellX + (float(c.x) + 0.5f) * kCell, kWellY + float(c.y + 1 - kHiddenH) * kCell },
                                     pc, 3, 110.0f, ParticleKind::Dust, 0.45f, 8.0f, 30.0f);
                }
                app.fx.shake(0.08f + 0.012f * float(e.value));
                break;
            }
            case EventType::Lock:
                if (!anyClear)
                    for (const Point c : e.cells)
                        app.fx.cellFlash(cellRect(c.x, float(c.y)), WHITE, 0.16f);
                app.audio.play(Sfx::Lock, 1.0f, 0.8f);
                break;
            case EventType::LineClear:
                onLineClear(app, e);
                break;
            case EventType::SpinNoLines:
                app.audio.play(Sfx::TSpin);
                app.fx.popup(e.spin == Spin::Mini ? "T-SPIN MINI" : "T-SPIN", { kWellCenter.x, kWellY + 200.0f }, 36,
                             { 196, 110, 255, 255 }, 1.1f, 0.0f, Face::Bold, 30.0f, Slot::ClearName);
                for (const Point c : e.cells)
                    app.fx.burst({ kWellX + (float(c.x) + 0.5f) * kCell, kWellY + (float(c.y - kHiddenH) + 0.5f) * kCell },
                                 { 196, 110, 255, 255 }, 5, 260.0f, ParticleKind::Spark, 0.5f, 5.0f, 60.0f);
                break;
            case EventType::LevelUp: {
                const Color c = hueColor(levelHue(e.value));
                app.audio.play(Sfx::LevelUp);
                app.fx.popup(fmt("LEVEL %d", e.value), { kWellCenter.x, kWellY + kWellH * 0.3f }, 44, c, 1.5f, 0.3f,
                             Face::Display, 30.0f, Slot::Banner);
                app.fx.ring(kWellCenter, 400.0f, c, 0.8f);
                break;
            }
            case EventType::TopOut:
                app.audio.play(Sfx::GameOver);
                enterEnding(app);
                break;
            case EventType::Finished:
                app.audio.play(Sfx::Finish);
                enterEnding(app);
                break;
            case EventType::ZenReset:
                app.fx.popup("CLEAN SLATE", kWellCenter, 44, { 196, 110, 255, 255 }, 1.4f, 0.0f, Face::Bold, 20.0f,
                             Slot::Banner);
                app.fx.flash({ 196, 110, 255, 255 }, 0.35f);
                app.fx.clearRowOffsets();
                break;
            default:
                break;
        }
    }
    events.clear();
}

void checkNewBest(App& app)
{
    if (app.newBestShown || app.mode == GameMode::Sprint)
        return;
    const ScoreEntry* best = app.book.best(app.mode);
    if (!best || best->score <= 0 || app.game->stats().score <= best->score)
        return;
    app.newBestShown = true;
    app.audio.play(Sfx::NewRecord);
    app.fx.popup("NEW BEST!", { kRightX + kSideW * 0.5f, kWellY + 560.0f }, 30, kGold, 2.2f, 0.0f, Face::Bold, 30.0f);
}

void finishRun(App& app)
{
    const Game& g = *app.game;
    const bool eligible = app.mode != GameMode::Sprint || g.completed();
    ScoreEntry e;
    e.score = g.stats().score;
    e.lines = g.stats().lines;
    e.level = g.stats().level;
    e.time = g.stats().time;
    app.newRank = eligible ? app.book.rankFor(app.mode, e) : -1;
    app.enteringName = app.newRank >= 0;
    app.name = app.book.prefs.lastName;
    goTo(app, Screen::Results);
}

// ---------------------------------------------------------------------------
//  Pause menu
// ---------------------------------------------------------------------------
std::vector<const char*> pauseItems(const App& app)
{
    if (app.mode == GameMode::Zen)
        return { "RESUME", "RESTART", "END SESSION", "MAIN MENU" };
    return { "RESUME", "RESTART", "MAIN MENU" };
}

Rectangle pauseItemRect(float i)
{
    return { kWellCenter.x - 130.0f, 290.0f + i * 56.0f, 260.0f, 46.0f };
}

void updatePause(App& app, float dt)
{
    const auto items = pauseItems(app);
    const int n = int(items.size());
    app.pauseHi += (float(app.pauseSel) - app.pauseHi) * std::min(1.0f, dt * 18.0f);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        resumeGame(app);
        return;
    }
    if (navPressed(KEY_UP)) {
        app.pauseSel = (app.pauseSel + n - 1) % n;
        app.audio.play(Sfx::MenuMove);
    }
    if (navPressed(KEY_DOWN)) {
        app.pauseSel = (app.pauseSel + 1) % n;
        app.audio.play(Sfx::MenuMove);
    }
    const Vector2 m = mouseVirtual(app);
    if (mouseMoved(app))
        for (int i = 0; i < n; ++i)
            if (CheckCollisionPointRec(m, pauseItemRect(float(i))) && app.pauseSel != i) {
                app.pauseSel = i;
                app.audio.play(Sfx::MenuMove);
            }
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, pauseItemRect(float(app.pauseSel)));
    if (!confirmPressed() && !click)
        return;
    const std::string item = items[std::size_t(app.pauseSel)];
    app.audio.play(Sfx::MenuSelect);
    if (item == "RESUME") {
        resumeGame(app);
    } else if (item == "RESTART") {
        startGame(app, app.mode);
    } else if (item == "END SESSION") {
        resumeGame(app);
        app.game->endSession();
    } else {
        app.audio.setMusicDuck(1.0f);
        goTo(app, Screen::ModeSelect);
    }
}

void drawPause(App& app, Pass pass, Color accent)
{
    const Rectangle panel{ kWellCenter.x - 230.0f, 176.0f, 460.0f, 410.0f };
    const float rn = roundness(panel, 22.0f);
    if (pass == Pass::Main) {
        DrawRectangle(-400, -400, int(kVirtualW) + 800, int(kVirtualH) + 800, fadeColor({ 4, 4, 14, 255 }, 0.6f));
        DrawRectangleRounded(panel, rn, 10, fadeColor({ 10, 12, 30, 255 }, 0.96f));
        DrawRectangleRoundedLinesEx(panel, rn, 10, 2.0f, fadeColor(accent, 0.7f));
    } else {
        DrawRectangleRoundedLinesEx(panel, rn, 10, 3.0f, fadeColor(accent, 0.35f));
    }
    app.r.text(Face::Display, "PAUSED", { kWellCenter.x, 200.0f }, 52, pass == Pass::Main ? kTextBright : fadeColor(accent, 0.8f),
               Align::Center, 12.0f);
    const auto items = pauseItems(app);
    const Rectangle hi = pauseItemRect(app.pauseHi);
    if (pass == Pass::Main) {
        DrawRectangleRounded(hi, roundness(hi, 10), 8, fadeColor(accent, 0.16f));
        DrawRectangleRoundedLinesEx(hi, roundness(hi, 10), 8, 1.5f, fadeColor(accent, 0.8f));
    } else {
        DrawRectangleRoundedLinesEx(hi, roundness(hi, 10), 8, 3.0f, fadeColor(accent, 0.6f));
    }
    for (int i = 0; i < int(items.size()); ++i) {
        const Rectangle r = pauseItemRect(float(i));
        const bool sel = i == app.pauseSel;
        if (pass == Pass::Main)
            app.r.text(Face::Bold, items[std::size_t(i)], { r.x + r.width * 0.5f, r.y + 8.0f }, 24,
                       sel ? kTextBright : kTextMuted, Align::Center, 3.0f);
    }
    drawHints(app, { { "Esc", "Resume" }, { "UP DOWN", "Choose" }, { "Enter", "Select" } }, panel.y + panel.height - 50.0f, pass);
}

// ---------------------------------------------------------------------------
//  HUD panels
// ---------------------------------------------------------------------------
void statBlock(App& app, Pass pass, const char* label, const std::string& value, float y, float size, Color color)
{
    if (pass == Pass::Main) {
        drawLabel(app, label, { kLeftX + 18.0f, y }, kTextMuted, Align::Left, 14.0f);
        app.r.textDigits(Face::Display, value, { kLeftX + 18.0f, y + 18.0f }, size, color);
    } else if (size > 40.0f) {
        app.r.textDigits(Face::Display, value, { kLeftX + 18.0f, y + 18.0f }, size, fadeColor(color, 0.35f));
    }
}

void drawStats(App& app, Pass pass, Color accent)
{
    const Game& g = *app.game;
    const Stats& s = g.stats();
    const Rectangle box{ kLeftX, kWellY + 146.0f, kSideW, kWellH - 146.0f };
    drawPanel(box, accent, 1.0f, pass);
    const float y = box.y + 18.0f;
    const std::string pps = fmt("%.2f", s.pps());
    switch (app.mode) {
        case GameMode::Marathon:
            statBlock(app, pass, "LEVEL", std::to_string(s.level), y, 46, accent);
            statBlock(app, pass, "LINES", std::to_string(s.lines), y + 88, 32, kTextBright);
            statBlock(app, pass, "TIME", clockTime(s.time, false), y + 160, 32, kTextBright);
            statBlock(app, pass, "PIECES / SEC", pps, y + 232, 32, kTextBright);
            break;
        case GameMode::Sprint:
            statBlock(app, pass, "LINES LEFT", std::to_string(g.linesLeft()), y, 46, accent);
            statBlock(app, pass, "TIME", clockTime(s.time, true), y + 88, 32, kTextBright);
            statBlock(app, pass, "PIECES / SEC", pps, y + 160, 32, kTextBright);
            statBlock(app, pass, "PIECES", std::to_string(s.pieces), y + 232, 32, kTextBright);
            break;
        case GameMode::Ultra: {
            const bool hurry = g.timeLeft() < 10.0 && !g.over();
            const Color c = hurry ? ColorLerp(accent, { 255, 70, 90, 255 }, 0.5f + 0.5f * std::sin(app.time * 12.0f)) : accent;
            statBlock(app, pass, "TIME LEFT", clockTime(g.timeLeft(), true), y, 46, c);
            statBlock(app, pass, "LEVEL", std::to_string(s.level), y + 88, 32, kTextBright);
            statBlock(app, pass, "LINES", std::to_string(s.lines), y + 160, 32, kTextBright);
            statBlock(app, pass, "PIECES / SEC", pps, y + 232, 32, kTextBright);
            break;
        }
        case GameMode::Zen:
            statBlock(app, pass, "LINES", std::to_string(s.lines), y, 46, accent);
            statBlock(app, pass, "TIME", clockTime(s.time, false), y + 88, 32, kTextBright);
            statBlock(app, pass, "PIECES / SEC", pps, y + 160, 32, kTextBright);
            statBlock(app, pass, "PIECES", std::to_string(s.pieces), y + 232, 32, kTextBright);
            break;
    }

    // live combo / back-to-back indicators
    const float iy = box.y + box.height - 104.0f;
    if (g.combo() >= 1) {
        const Color c{ 255, 150, 40, 255 };
        if (pass == Pass::Main)
            drawLabel(app, "COMBO", { kLeftX + 18.0f, iy }, kTextMuted, Align::Left, 14.0f);
        app.r.text(Face::Bold, fmt("x%d", g.combo()), { kLeftX + 18.0f, iy + 16.0f }, 28, pass == Pass::Main ? c : fadeColor(c, 0.5f));
    }
    if (s.b2bStreak >= 1 && g.backToBack()) {
        if (pass == Pass::Main)
            drawLabel(app, "BACK-TO-BACK", { kLeftX + kSideW - 18.0f, iy }, kTextMuted, Align::Right, 14.0f);
        app.r.text(Face::Bold, fmt("x%d", s.b2bStreak), { kLeftX + kSideW - 18.0f, iy + 16.0f }, 28,
                   pass == Pass::Main ? kGold : fadeColor(kGold, 0.5f), Align::Right);
    }
}

void drawHold(App& app, Pass pass, Color accent, bool hide)
{
    const Game& g = *app.game;
    const Rectangle box{ kLeftX, kWellY, kSideW, 130.0f };
    drawPanel(box, accent, 1.0f, pass);
    if (pass == Pass::Glow && app.holdFlash > 0.0f)
        DrawRectangleRoundedLinesEx(box, roundness(box, 14), 10, 4.0f, fadeColor(accent, app.holdFlash));
    if (pass == Pass::Main)
        drawLabel(app, "HOLD", { box.x + 18.0f, box.y + 14.0f }, kTextMuted);
    if (g.held() && !hide)
        drawPieceIcon(app, *g.held(), { box.x + box.width * 0.5f, box.y + 78.0f }, 26.0f, 1.0f, pass, !g.canHold());
}

void drawNext(App& app, Pass pass, Color accent, bool hide)
{
    const Game& g = *app.game;
    const Rectangle box{ kRightX, kWellY, kSideW, 370.0f };
    drawPanel(box, accent, 1.0f, pass);
    if (pass == Pass::Main)
        drawLabel(app, "NEXT", { box.x + 18.0f, box.y + 14.0f }, kTextMuted);
    if (hide)
        return;
    const float cx = box.x + box.width * 0.5f;
    drawPieceIcon(app, g.next(0), { cx, box.y + 84.0f }, 26.0f, 1.0f, pass, false);
    for (int i = 1; i < cfg::kPreviewCount; ++i)
        drawPieceIcon(app, g.next(i), { cx, box.y + 150.0f + float(i - 1) * 56.0f }, 19.0f, 0.85f, pass, false);
}

void drawScore(App& app, Pass pass, Color accent)
{
    const Rectangle box{ kRightX, kWellY + 386.0f, kSideW, kWellH - 386.0f };
    drawPanel(box, accent, 1.0f, pass);
    const float x = box.x + 18.0f;
    const std::string score = withCommas((long long)std::llround(app.shownScore));
    if (pass == Pass::Main) {
        drawLabel(app, "SCORE", { x, box.y + 16.0f }, kTextMuted, Align::Left, 14.0f);
        app.r.textDigits(Face::Display, score, { x, box.y + 34.0f }, score.size() > 9 ? 26.0f : 32.0f, kTextBright);
        drawLabel(app, "BEST", { x, box.y + 96.0f }, kTextMuted, Align::Left, 14.0f);
    } else {
        app.r.textDigits(Face::Display, score, { x, box.y + 34.0f }, score.size() > 9 ? 26.0f : 32.0f, fadeColor(accent, 0.25f));
    }
    if (pass == Pass::Main) {
        const ScoreEntry* best = app.book.best(app.mode);
        std::string text = "---";
        if (best)
            text = app.mode == GameMode::Sprint ? clockTime(best->time, true) : withCommas(best->score);
        app.r.textDigits(Face::Display, text, { x, box.y + 114.0f }, 24, kTextMuted);
        drawLabel(app, fmt("LV %d START", app.game->startLevel()), { x, box.y + 160.0f }, kTextDim, Align::Left, 13.0f);
    }
}

} // namespace

// ---------------------------------------------------------------------------
//  Shared board drawing (also used by the title-screen demo and results)
// ---------------------------------------------------------------------------
void drawBoard(App& app, const Game& g, const BoardView& v, Pass pass)
{
    Renderer& r = app.r;
    const float cell = v.cell;
    const bool glow = pass == Pass::Glow;
    const Rectangle well{ v.origin.x, v.origin.y, cell * kBoardW, cell * kVisibleH };
    const float pad = cell * 0.2f;
    const Rectangle frame{ well.x - pad, well.y - pad, well.width + pad * 2.0f, well.height + pad * 2.0f };
    const float pulse = 0.5f + 0.5f * std::sin(app.time * 6.0f);
    const Color border = ColorLerp(v.accent, { 255, 50, 80, 255 }, v.danger * (0.6f + 0.4f * pulse));

    if (!glow) {
        DrawRectangleRounded(frame, roundness(frame, cell * 0.33f), 8, fadeColor({ 6, 8, 22, 255 }, 0.88f * v.alpha));
        const Color line = fadeColor(WHITE, 0.05f * v.alpha);
        for (int x = 1; x < kBoardW; ++x)
            DrawLineEx({ well.x + float(x) * cell, well.y }, { well.x + float(x) * cell, well.y + well.height }, 1.0f, line);
        for (int y = 1; y < kVisibleH; ++y)
            DrawLineEx({ well.x, well.y + float(y) * cell }, { well.x + well.width, well.y + float(y) * cell }, 1.0f, line);
        if (v.danger > 0.0f)
            DrawRectangleGradientV(int(well.x), int(well.y), int(well.width), int(well.height * 0.4f),
                                   fadeColor({ 255, 40, 70, 255 }, 0.22f * v.danger * (0.6f + 0.4f * pulse)), BLANK);
    }
    DrawRectangleRoundedLinesEx(frame, roundness(frame, cell * 0.33f), 8, glow ? 3.0f : 2.0f,
                                fadeColor(border, (glow ? 0.9f : 0.85f) * v.alpha));

    if (v.hideBlocks)
        return;
    auto cellAt = [&](int x, float y) {
        return Rectangle{ well.x + float(x) * cell, well.y + (y - float(kHiddenH)) * cell, cell, cell };
    };
    const Board& b = g.board();
    for (int y = kHiddenH - 3; y < kBoardH; ++y) {
        const float off = v.useFx ? app.fx.rowOffset(y) : 0.0f;
        const bool greyed = y >= kBoardH - v.greyRows;
        const float a = v.alpha * (y < kHiddenH ? 0.6f : 1.0f);
        for (int x = 0; x < kBoardW; ++x) {
            const int c = b[std::size_t(y)][std::size_t(x)];
            if (c == 0)
                continue;
            const int t = (greyed || c == kGreyCell) ? kTileGrey : c - 1;
            const Rectangle rc = cellAt(x, float(y) + off);
            if (!glow)
                r.tile(t, rc, fadeColor(WHITE, a));
            else
                r.tile(kTileWhite, rc, fadeColor(Renderer::pieceColor(t), (t == kTileGrey ? 0.04f : 0.14f) * a));
        }
    }
    if (g.over())
        return;

    const ActivePiece& p = g.active();
    const Color pc = Renderer::pieceColor(int(p.type));
    ActivePiece ghost = p;
    ghost.y = g.ghostY();
    if (ghost.y != p.y)
        for (const Point c : cellsOf(ghost))
            r.tile(kTileGhost, cellAt(c.x, float(c.y)), fadeColor(pc, (glow ? 0.25f : 0.6f) * v.alpha));

    const float fall = cfg::kSmoothFall ? float(g.fallProgress()) : 0.0f;
    const float lock = float(g.lockProgress());
    for (const Point c : g.activeCells()) {
        const Rectangle rc = cellAt(c.x, float(c.y) + fall);
        if (!glow) {
            r.tile(int(p.type), rc, fadeColor(WHITE, v.alpha));
            if (lock > 0.0f)
                r.tile(kTileWhite, rc, fadeColor(WHITE, lock * 0.4f * v.alpha));
        } else {
            r.tile(kTileWhite, rc, fadeColor(pc, 0.35f * v.alpha));
        }
    }
}

void drawPieceIcon(App& app, PieceType type, Vector2 center, float cell, float alpha, Pass pass, bool dim)
{
    const Cells& cs = shapeCells(type, 0);
    int minX = 9, maxX = -9, minY = 9, maxY = -9;
    for (const Point c : cs) {
        minX = std::min(minX, c.x);
        maxX = std::max(maxX, c.x);
        minY = std::min(minY, c.y);
        maxY = std::max(maxY, c.y);
    }
    const float w = float(maxX - minX + 1) * cell, h = float(maxY - minY + 1) * cell;
    for (const Point c : cs) {
        const Rectangle rc{ center.x - w * 0.5f + float(c.x - minX) * cell, center.y - h * 0.5f + float(c.y - minY) * cell, cell, cell };
        if (pass == Pass::Main)
            app.r.tile(dim ? kTileGrey : int(type), rc, fadeColor(WHITE, alpha));
        else
            app.r.tile(kTileWhite, rc, fadeColor(Renderer::pieceColor(int(type)), (dim ? 0.04f : 0.28f) * alpha));
    }
}

// ---------------------------------------------------------------------------
//  Screen entry points
// ---------------------------------------------------------------------------
void startGame(App& app, GameMode mode)
{
    app.mode = mode;
    const auto seed = app.autotest ? std::uint64_t(2024)
                                   : std::uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const int startLevel = mode == GameMode::Marathon ? app.book.prefs.startLevel : 1;
    app.game = std::make_unique<Game>(mode, startLevel, seed);
    app.game->events().clear();
    app.book.prefs.lastMode = int(mode);
    app.book.savePrefs();
    goTo(app, Screen::Play);
}

void enterPlay(App& app)
{
    app.phase = Phase::Countdown;
    app.phaseTime = 0.0f;
    app.countdownStep = -1;
    app.shownScore = 0.0;
    app.newBestShown = false;
    app.greyRows = 0;
    app.holdFlash = 0.0f;
    app.lastClearLines = 0;
    app.fx.reset();
    app.input.reset();
    app.testBot = AutoPlayer(30.0);
    app.audio.setSong(Song::Game);
    app.audio.setTempo(1.0f);
    app.audio.setMusicDuck(1.0f);

    // Rasterize the gameplay fonts now, while the screen is still faded out,
    // so the first countdown number or TETRIS popup never causes a hitch.
    for (float s : { 120.0f, 104.0f, 52.0f, 46.0f, 44.0f, 32.0f, 28.0f, 26.0f, 24.0f })
        app.r.prewarm(Face::Display, s);
    for (float s : { 62.0f, 54.0f, 40.0f, 36.0f, 30.0f, 28.0f, 24.0f })
        app.r.prewarm(Face::Bold, s);
    for (float s : { 22.0f, 15.0f, 14.0f, 13.0f })
        app.r.prewarm(Face::Ui, s);
}

void pauseGame(App& app)
{
    if (app.phase != Phase::Live)
        return;
    app.phase = Phase::Paused;
    app.pauseSel = 0;
    app.pauseHi = 0.0f;
    app.audio.play(Sfx::Pause);
    app.audio.setMusicDuck(0.3f);
    app.input.reset();
}

void resumeGame(App& app)
{
    app.phase = Phase::Live;
    app.audio.setMusicDuck(1.0f);
    app.input.reset();
}

void updatePlay(App& app, float dt)
{
    Game& g = *app.game;
    app.phaseTime += dt;
    app.softDropSfxTimer -= dt;
    app.holdFlash = std::max(0.0f, app.holdFlash - dt * 3.0f);

    switch (app.phase) {
        case Phase::Countdown: {
            if (!app.autotest)
                app.input.poll(dt);  // DAS can charge before GO
            const int step = int(app.phaseTime / kCountStep);
            if (step != app.countdownStep) {
                app.countdownStep = step;
                const Vector2 at{ kWellCenter.x, kWellCenter.y - 20.0f };
                if (step < 3) {
                    app.audio.play(Sfx::Countdown);
                    app.fx.popup(std::to_string(3 - step), at, 120, gameAccent(app), kCountStep, 0.0f, Face::Display, 0.0f,
                                 Slot::Center);
                } else {
                    app.audio.play(Sfx::Go);
                    app.fx.popup("GO!", at, 104, kTextBright, 0.7f, 0.0f, Face::Display, 10.0f, Slot::Center);
                    app.phase = Phase::Live;
                    app.phaseTime = 0.0f;
                }
            }
            break;
        }
        case Phase::Live: {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || (!app.autotest && !IsWindowFocused())) {
                pauseGame(app);
                break;
            }
            const InputFrame in = app.autotest ? app.testBot.step(g, dt) : app.input.poll(dt);
            g.update(dt, in);
            handleEvents(app);
            checkNewBest(app);
            break;
        }
        case Phase::Paused:
            updatePause(app, dt);
            break;
        case Phase::Ending:
            handleEvents(app);
            if (g.toppedOut())
                app.greyRows = std::min(kVisibleH + 3, int(app.phaseTime / 0.045f));
            if (app.phaseTime > (g.toppedOut() ? 2.0f : 2.4f))
                finishRun(app);
            break;
    }

    // rolling score counter
    const double target = double(g.stats().score);
    app.shownScore += (target - app.shownScore) * (1.0 - std::exp(-double(dt) * 10.0));
    if (std::fabs(target - app.shownScore) < 1.0)
        app.shownScore = target;

    // the music speeds up with the level, and a little more under pressure
    float tempo = std::min(1.0f + 0.035f * float(g.stats().level - 1), 1.55f);
    if (app.mode == GameMode::Zen)
        tempo = 0.92f;
    if (app.mode == GameMode::Ultra && g.timeLeft() < 20.0)
        tempo *= 1.12f;
    if (app.danger > 0.5f)
        tempo *= 1.08f;
    app.audio.setTempo(tempo);
}

void drawPlay(App& app, Pass pass)
{
    const Game& g = *app.game;
    const Color accent = gameAccent(app);
    const bool paused = app.phase == Phase::Paused;
    if (paused && pass == Pass::Glow) {  // bloom is added on top of everything: only the menu may glow
        drawPause(app, pass, accent);
        return;
    }

    app.r.text(Face::Display, modeName(app.mode), { kLeftX, 34.0f }, 28, pass == Pass::Main ? accent : fadeColor(accent, 0.5f),
               Align::Left, 6.0f);
    if (pass == Pass::Main)
        drawLabel(app, "ESC  PAUSE", { kRightX + kSideW, 44.0f }, kTextDim, Align::Right, 13.0f);

    BoardView v;
    v.origin = { kWellX, kWellY };
    v.cell = kCell;
    v.useFx = true;
    v.hideBlocks = paused;
    v.greyRows = app.greyRows;
    v.accent = accent;
    v.danger = app.danger;
    drawBoard(app, g, v, pass);

    drawHold(app, pass, accent, paused);
    drawNext(app, pass, accent, paused);
    drawStats(app, pass, accent);
    drawScore(app, pass, accent);

    if (paused) {
        drawPause(app, pass, accent);
        return;
    }
    app.fx.drawWorld(app.r, pass);
    app.fx.drawPopups(app.r, pass);
}
