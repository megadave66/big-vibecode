#pragma once
// Player progress: attempts, best percent and completion per level. Pure logic plus file I/O;
// the save path is injected (the app picks it). A missing or corrupt file means "start fresh".

#include <map>
#include <utility>
#include <string>

namespace gd {

struct LevelProgress {
    int attempts = 0;
    int bestPercentNormal = 0;     // 0..100
    int bestPercentPractice = 0;   // 0..100
    bool completed = false;        // set by a normal-mode win only
};

class Progress {
public:
    Progress() = default;
    explicit Progress(std::string path) : path_(std::move(path)) {}

    const std::string& path() const { return path_; }
    void setPath(std::string p) { path_ = std::move(p); }

    // Read the file. Returns true when a valid file was loaded. On a missing or corrupt file the
    // data is cleared and false is returned (never throws).
    bool load();
    // Write to "<path>.tmp", then rename over the real file. Returns false on failure.
    // An empty path means "do not persist" and returns true.
    bool save() const;

    // Level ids <= 0 are ignored.
    void recordAttempt(int levelId);
    // Best percent only goes up. Values are clamped to 0..100.
    void recordBest(int levelId, bool practice, int percent);
    // Win: best = 100 for that mode; completed is set for normal mode only.
    void recordWin(int levelId, bool practice);

    LevelProgress get(int levelId) const;
    const std::map<int, LevelProgress>& all() const { return levels_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

private:
    std::string path_;
    std::map<int, LevelProgress> levels_;
    mutable bool dirty_ = false;
};

}  // namespace gd
