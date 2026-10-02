#include "Menus.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/LevelLoader.h"

namespace gd::menu {

namespace {

constexpr float kW = 1280.0f;
constexpr float kH = 720.0f;
constexpr double kPi = 3.14159265358979323846;

const Rgba kGold = rgba(255, 220, 90);
const Rgba kWhite = rgba(255, 255, 255);
const Rgba kDim = rgba(150, 175, 220);
const Rgba kLight = rgba(215, 230, 255);
const Rgba kInk = rgba(18, 24, 64);
const Rgba kGreen = rgba(90, 235, 120);

void background(Gfx& g, double time) {
    g.gradientRect(0, 0, kW, kH, rgba(18, 26, 84), rgba(44, 96, 180));
    // Slow drifting squares for a bit of life.
    for (int i = 0; i < 14; ++i) {
        const double speed = 14.0 + (i % 5) * 7.0;
        const float size = 24.0f + static_cast<float>((i * 37) % 50);
        const float x = static_cast<float>(std::fmod(i * 197.0 + time * speed, kW + 200.0)) - 100.0f;
        const float y = static_cast<float>((i * 131) % 600) + 20.0f;
        g.fillRect(x, y, size, size, rgba(255, 255, 255, 14));
    }
}

void button(Gfx& g, TextRenderer& t, const Rect& r, const std::string& label, bool selected, int size = 36,
            bool enabled = true) {
    const Rgba fill = selected ? kGold : rgba(10, 18, 60, 190);
    const Rgba edge = selected ? rgba(255, 255, 255) : rgba(110, 150, 220, 200);
    g.fillRect(r.x, r.y, r.w, r.h, fill);
    g.outlineRect(r.x, r.y, r.w, r.h, 3.0f, edge);
    float tw = 0, th = 0;
    t.measure(label, size, tw, th);
    const Rgba col = !enabled ? rgba(110, 125, 160) : (selected ? kInk : kWhite);
    t.draw(g, label, r.x + r.w * 0.5f, r.y + (r.h - th) * 0.5f, size, col, TextAlign::Center, !selected);
}

void star(Gfx& g, float cx, float cy, float radius, Rgba c) {
    Pt pts[10];
    for (int i = 0; i < 10; ++i) {
        const double a = -kPi / 2.0 + i * kPi / 5.0;
        const float r = (i % 2 == 0) ? radius : radius * 0.45f;
        pts[i] = {cx + static_cast<float>(std::cos(a)) * r, cy + static_cast<float>(std::sin(a)) * r};
    }
    for (int i = 0; i < 10; ++i) g.fillTriangle({cx, cy}, pts[i], pts[(i + 1) % 10], c);
}

void tick(Gfx& g, float cx, float cy, float r) {
    g.fillEllipse(cx, cy, r, r, kGreen);
    g.line(cx - r * 0.5f, cy, cx - r * 0.1f, cy + r * 0.45f, 3.0f, kInk);
    g.line(cx - r * 0.1f, cy + r * 0.45f, cx + r * 0.55f, cy - r * 0.4f, 3.0f, kInk);
}

std::string pct(int v) { return std::to_string(v) + "%"; }

}  // namespace

std::vector<LevelInfo> loadLevelInfos(const std::string& dataDir) {
    std::vector<LevelInfo> out;
    for (int id = 1; id <= kLevelCount; ++id) {
        LevelInfo info;
        info.id = id;
        std::vector<std::string> errs;
        if (auto lvl = loadLevelFromFile(levelPath(dataDir, id), errs)) {
            info.name = lvl->name;
            info.difficulty = lvl->difficulty;
            info.available = true;
        }
        out.push_back(std::move(info));
    }
    return out;
}

// ------------------------------------------------------------------ layout

Rect mainButton(int i) { return {460.0f, 330.0f + 96.0f * static_cast<float>(i), 360.0f, 74.0f}; }

Rect levelCard(int i) {
    const int col = i % kLevelColumns, row = i / kLevelColumns;
    return {30.0f + 620.0f * static_cast<float>(col), 98.0f + 104.0f * static_cast<float>(row), 600.0f, 96.0f};
}

Rect practiceButton() { return {930.0f, 22.0f, 320.0f, 54.0f}; }
Rect backButton() { return {30.0f, 22.0f, 150.0f, 54.0f}; }
Rect pauseButton(int i) { return {450.0f, 274.0f + 64.0f * static_cast<float>(i), 380.0f, 54.0f}; }
Rect completeButton(int i) { return {340.0f + 310.0f * static_cast<float>(i), 560.0f, 290.0f, 66.0f}; }

// ------------------------------------------------------------------ main menu

void drawMainMenu(Gfx& g, TextRenderer& t, int selected, double time) {
    background(g, time);
    t.draw(g, "GEOMETRY DASH", kW / 2, 100, 96, kGold, TextAlign::Center);
    t.draw(g, "REMAKE", kW / 2, 212, 40, kLight, TextAlign::Center);
    static const char* labels[kMainButtons] = {"Play", "Quit"};
    for (int i = 0; i < kMainButtons; ++i) button(g, t, mainButton(i), labels[i], i == selected, 40);

    // Ground strip with a cube that hops along it.
    const float groundY = 640.0f;
    g.fillRect(0, groundY, kW, kH - groundY, rgba(12, 22, 70));
    g.fillRect(0, groundY, kW, 4, rgba(95, 208, 255));
    const double cycle = 1.1;
    const double ph = std::fmod(time, cycle) / cycle;
    const float hop = static_cast<float>(std::max(0.0, std::sin(ph * kPi * 1.0))) * 70.0f;
    const float cx = 180.0f, size = 44.0f;
    const float cy = groundY - size - hop;
    const double ang = ph * 90.0 * kPi / 180.0;
    const float hs = size * 0.5f;
    Pt c[4];
    const Pt ctr{cx + hs, cy + hs};
    const Pt base[4] = {{-hs, -hs}, {hs, -hs}, {hs, hs}, {-hs, hs}};
    for (int i = 0; i < 4; ++i) {
        const float ca = static_cast<float>(std::cos(ang)), sa = static_cast<float>(std::sin(ang));
        c[i] = {ctr.x + base[i].x * ca - base[i].y * sa, ctr.y + base[i].x * sa + base[i].y * ca};
    }
    g.fillPoly(c, 4, kGold);
    g.outlinePoly(c, 4, 3.0f, rgba(255, 255, 255));
    t.draw(g, "Arrows / mouse  -  Enter to select", kW / 2, 664, 22, kLight, TextAlign::Center);
    g.flush();
}

// ------------------------------------------------------------------ level select

void drawLevelSelect(Gfx& g, TextRenderer& t, const std::vector<LevelInfo>& levels, const Progress& progress,
                     int selected, bool practice, double time, const std::string& toast) {
    background(g, time);
    button(g, t, backButton(), "< Back", false, 24);
    t.draw(g, "SELECT LEVEL", kW / 2, 22, 52, kWhite, TextAlign::Center);
    {
        const Rect r = practiceButton();
        const std::string label = practice ? "Mode: PRACTICE  (P)" : "Mode: NORMAL  (P)";
        const Rgba fill = practice ? rgba(60, 200, 110, 230) : rgba(10, 18, 60, 190);
        g.fillRect(r.x, r.y, r.w, r.h, fill);
        g.outlineRect(r.x, r.y, r.w, r.h, 3.0f, practice ? rgba(200, 255, 220) : rgba(110, 150, 220, 200));
        float tw = 0, th = 0;
        t.measure(label, 22, tw, th);
        t.draw(g, label, r.x + r.w / 2, r.y + (r.h - th) / 2, 22, practice ? kInk : kWhite, TextAlign::Center,
               !practice);
    }

    for (int i = 0; i < kLevelCount; ++i) {
        const Rect r = levelCard(i);
        const bool sel = i == selected;
        const LevelInfo* info = i < static_cast<int>(levels.size()) ? &levels[static_cast<std::size_t>(i)] : nullptr;
        const bool ok = info && info->available;
        g.fillRect(r.x, r.y, r.w, r.h, sel ? rgba(30, 50, 130, 245) : rgba(8, 16, 56, 200));
        g.outlineRect(r.x, r.y, r.w, r.h, sel ? 4.0f : 2.0f, sel ? kGold : rgba(90, 130, 200, 170));
        if (sel) g.fillRect(r.x, r.y, 8, r.h, kGold);

        // Number.
        t.draw(g, std::to_string(i + 1), r.x + 50, r.y + 24, 44, ok ? kGold : kDim, TextAlign::Center);
        if (!ok) {
            t.draw(g, "Level " + std::to_string(i + 1) + " - missing", r.x + 98, r.y + 32, 26, rgba(160, 170, 200));
            continue;
        }
        t.draw(g, info->name, r.x + 98, r.y + 10, 28, kWhite);
        // Difficulty: 10 small stars, filled up to the level's difficulty.
        const int d = std::clamp(info->difficulty, 1, 10);
        const Rgba lit = mix(rgba(120, 235, 120), rgba(255, 90, 90), (d - 1) / 9.0);
        for (int s = 0; s < 10; ++s)
            star(g, r.x + 110 + 22.0f * static_cast<float>(s), r.y + 68, 10.0f, s < d ? lit : rgba(70, 90, 140, 200));

        const LevelProgress lp = progress.get(info->id);
        const float cols[3] = {r.x + 390, r.x + 478, r.x + 556};
        const char* labels[3] = {"NORMAL", "PRACTICE", "TRIES"};
        const std::string values[3] = {pct(lp.bestPercentNormal), pct(lp.bestPercentPractice),
                                       std::to_string(lp.attempts)};
        for (int c = 0; c < 3; ++c) {
            t.draw(g, labels[c], cols[c], r.y + 12, 15, kDim, TextAlign::Center);
            Rgba vc = kWhite;
            if (c == 0 && lp.completed) vc = kGreen;
            t.draw(g, values[c], cols[c], r.y + 34, 26, vc, TextAlign::Center);
        }
        if (lp.completed) tick(g, r.x + 556, r.y + 76, 10.0f);
    }

    if (!toast.empty()) t.draw(g, toast, kW / 2, 616, 24, rgba(255, 150, 150), TextAlign::Center);
    t.draw(g, "Arrows / mouse - Enter or click to play - P practice - Esc back", kW / 2, 668, 22, kLight,
           TextAlign::Center);
    g.flush();
}

// ------------------------------------------------------------------ pause

void drawPause(Gfx& g, TextRenderer& t, const GameScreen& game, int selected) {
    g.fillRect(0, 0, kW, kH, rgba(0, 0, 0, 160));
    t.draw(g, "PAUSED", kW / 2, 120, 80, kWhite, TextAlign::Center);
    std::string sub = game.level().name + "  -  " + std::to_string(game.percent()) + "%";
    if (game.practiceMode()) sub += "  -  PRACTICE";
    t.draw(g, sub, kW / 2, 218, 26, kLight, TextAlign::Center);
    const std::string labels[kPauseButtons] = {"Resume", "Restart",
                                               game.practiceMode() ? "Practice: ON" : "Practice: OFF",
                                               "Quit to menu"};
    g.fillRect(430, 258, 420, 316, rgba(0, 0, 0, 110));
    for (int i = 0; i < kPauseButtons; ++i) button(g, t, pauseButton(i), labels[i], i == selected, 30);
    t.draw(g, "Esc resume - R restart - P practice", kW / 2, 534, 18, kDim, TextAlign::Center);
    g.flush();
}

// ------------------------------------------------------------------ complete

void drawComplete(Gfx& g, TextRenderer& t, const GameScreen& game, const LevelProgress& saved, int selected) {
    g.fillRect(0, 0, kW, kH, rgba(0, 0, 0, 150));
    t.draw(g, "LEVEL COMPLETE", kW / 2, 70, 80, kGold, TextAlign::Center);
    t.draw(g, game.level().name, kW / 2, 176, 36, kWhite, TextAlign::Center);
    if (game.practiceMode()) t.draw(g, "PRACTICE MODE  (not counted as complete)", kW / 2, 222, 22, rgba(120, 235, 150), TextAlign::Center);

    char timeBuf[32];
    std::snprintf(timeBuf, sizeof timeBuf, "%.1f s", game.makeResult(true).seconds);
    const std::string rows[][2] = {{"Attempts", std::to_string(game.attempts())},
                                   {"Jumps", std::to_string(game.jumps())},
                                   {"Time", timeBuf},
                                   {"Best (normal)", pct(saved.bestPercentNormal)}};
    for (int i = 0; i < 4; ++i) {
        const float y = 280.0f + 58.0f * static_cast<float>(i);
        g.fillRect(380, y, 520, 48, rgba(8, 16, 56, 190));
        t.draw(g, rows[i][0], 404, y + 8, 28, kLight);
        t.draw(g, rows[i][1], 876, y + 8, 28, kWhite, TextAlign::Right);
    }
    button(g, t, completeButton(0), "Continue", selected == 0, 32);
    button(g, t, completeButton(1), "Retry", selected == 1, 32);
    g.flush();
}

}  // namespace gd::menu
