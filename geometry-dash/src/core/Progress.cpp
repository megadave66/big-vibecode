#include "Progress.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace gd {

namespace {
int clampPercent(int p) { return std::clamp(p, 0, 100); }
}  // namespace

bool Progress::load() {
    levels_.clear();
    dirty_ = false;
    if (path_.empty()) return false;
    std::ifstream in(path_, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    try {
        const nlohmann::json j = nlohmann::json::parse(ss.str());
        if (!j.is_object() || !j.contains("levels") || !j["levels"].is_object()) return false;
        std::map<int, LevelProgress> tmp;
        for (auto it = j["levels"].begin(); it != j["levels"].end(); ++it) {
            int id = 0;
            try {
                std::size_t used = 0;
                id = std::stoi(it.key(), &used);
                if (used != it.key().size()) continue;
            } catch (...) {
                continue;
            }
            if (id <= 0 || !it.value().is_object()) continue;
            const auto& o = it.value();
            LevelProgress lp;
            if (o.contains("attempts") && o["attempts"].is_number_integer())
                lp.attempts = std::max(0, o["attempts"].get<int>());
            if (o.contains("best_normal") && o["best_normal"].is_number_integer())
                lp.bestPercentNormal = clampPercent(o["best_normal"].get<int>());
            if (o.contains("best_practice") && o["best_practice"].is_number_integer())
                lp.bestPercentPractice = clampPercent(o["best_practice"].get<int>());
            if (o.contains("completed") && o["completed"].is_boolean()) lp.completed = o["completed"].get<bool>();
            tmp[id] = lp;
        }
        levels_ = std::move(tmp);
        return true;
    } catch (...) {
        levels_.clear();
        return false;
    }
}

bool Progress::save() const {
    if (path_.empty()) return true;
    nlohmann::json levels = nlohmann::json::object();
    for (const auto& [id, lp] : levels_) {
        levels[std::to_string(id)] = {{"attempts", lp.attempts},
                                      {"best_normal", lp.bestPercentNormal},
                                      {"best_practice", lp.bestPercentPractice},
                                      {"completed", lp.completed}};
    }
    const nlohmann::json j = {{"format", 1}, {"levels", levels}};
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target(path_);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    const std::string tmp = path_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << j.dump(1) << "\n";
        out.flush();
        if (!out) return false;
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    dirty_ = false;
    return true;
}

void Progress::recordAttempt(int levelId) {
    if (levelId <= 0) return;
    ++levels_[levelId].attempts;
    dirty_ = true;
}

void Progress::recordBest(int levelId, bool practice, int percent) {
    if (levelId <= 0) return;
    LevelProgress& lp = levels_[levelId];
    int& best = practice ? lp.bestPercentPractice : lp.bestPercentNormal;
    const int p = clampPercent(percent);
    if (p > best) {
        best = p;
        dirty_ = true;
    }
}

void Progress::recordWin(int levelId, bool practice) {
    if (levelId <= 0) return;
    recordBest(levelId, practice, 100);
    if (!practice) {
        LevelProgress& lp = levels_[levelId];
        if (!lp.completed) {
            lp.completed = true;
            dirty_ = true;
        }
    }
}

LevelProgress Progress::get(int levelId) const {
    const auto it = levels_.find(levelId);
    return it == levels_.end() ? LevelProgress{} : it->second;
}

}  // namespace gd
