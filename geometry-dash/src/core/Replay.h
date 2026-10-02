#pragma once
// Input replay files (format in docs/LEVEL-FORMAT.md) and a headless runner.
// Owner: physics section.

#include <optional>
#include <string>
#include <vector>

#include "core/Level.h"

namespace gd {

struct ReplayInput {
    int tick = 0;        // held state changes at the start of this tick (0-based)
    bool press = true;   // true = press, false = release
};

struct Replay {
    int version = 1;
    int level = 0;
    int tickRate = 0;                 // must equal phys::kTickRate
    std::vector<ReplayInput> inputs;  // strictly increasing ticks, alternating press/release
    int endTick = 0;                  // run must reach the end wall by this tick count
};

// Strict parse. On failure returns nullopt and appends "line N: message" errors.
std::optional<Replay> parseReplay(const std::string& text, std::vector<std::string>& errors);
std::optional<Replay> loadReplayFromFile(const std::string& path, std::vector<std::string>& errors);
std::string writeReplay(const Replay& replay);
bool saveReplayToFile(const Replay& replay, const std::string& path);

// Build a replay from a held-per-tick script (index = tick).
Replay replayFromHeld(int levelId, const std::vector<bool>& heldPerTick, int endTick);
// Held state for each tick 0..endTick-1.
std::vector<bool> heldPerTick(const Replay& replay);

struct ReplayResult {
    bool won = false;
    bool died = false;
    int tick = 0;     // ticks stepped when the run stopped
    double x = 0;     // player x when the run stopped
};

// Run the replay from a fresh Sim. Stops on win, death, or after endTick ticks.
ReplayResult runReplay(const Level& level, const Replay& replay);

}  // namespace gd
