#pragma once
// Menu screens: main menu, level select, pause, level complete. Procedural art, no SDL state of
// its own. The App owns the selection index and input; this module owns layout (so mouse
// hit-testing and drawing always agree) and drawing.

#include <string>
#include <vector>

#include "Gfx.h"
#include "GameScreen.h"
#include "Text.h"
#include "core/Progress.h"

namespace gd::menu {

constexpr int kLevelCount = 10;
constexpr int kMainButtons = 2;      // Play, Quit
constexpr int kPauseButtons = 4;     // Resume, Restart, Practice, Quit to menu
constexpr int kCompleteButtons = 2;  // Continue, Retry
constexpr int kLevelColumns = 2;

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct LevelInfo {
    int id = 0;
    std::string name;
    int difficulty = 1;
    bool available = false;   // false = file missing or unreadable
};

// Reads levels/level01..10.json for names and difficulty. Missing files give available = false.
std::vector<LevelInfo> loadLevelInfos(const std::string& dataDir);

Rect mainButton(int i);
Rect levelCard(int i);        // i in 0..kLevelCount-1
Rect practiceButton();        // level select
Rect backButton();            // level select
Rect pauseButton(int i);
Rect completeButton(int i);

void drawMainMenu(Gfx& g, TextRenderer& t, int selected, double time);
void drawLevelSelect(Gfx& g, TextRenderer& t, const std::vector<LevelInfo>& levels, const Progress& progress,
                     int selected, bool practice, double time, const std::string& toast);
// Pause and complete are drawn over a frozen game frame.
void drawPause(Gfx& g, TextRenderer& t, const GameScreen& game, int selected);
void drawComplete(Gfx& g, TextRenderer& t, const GameScreen& game, const LevelProgress& saved, int selected);

}  // namespace gd::menu
