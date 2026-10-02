// gd_replay <level.json> <file.replay> [--data DIR]
// Re-runs a replay headless. Prints one result line. Exit 0 only if the player reaches the end
// wall by the replay's `end` tick. Exit 1 = run failed, 2 = bad arguments or files.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/LevelLoader.h"
#include "core/Replay.h"

namespace {

int usage() {
    std::fprintf(stderr, "usage: gd_replay <level.json> <file.replay> [--data DIR]\n");
    return 2;
}

// Relative paths that do not exist from the working directory are tried under the data dir.
std::string resolve(const std::string& p, const std::string& dataDir) {
    namespace fs = std::filesystem;
    if (fs::exists(p) || dataDir.empty() || fs::path(p).is_absolute()) return p;
    const fs::path alt = fs::path(dataDir) / p;
    return fs::exists(alt) ? alt.string() : p;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> pos;
    std::string dataDir = GD_DATA_DIR;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data") {
            if (i + 1 >= argc) return usage();
            dataDir = argv[++i];
        } else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option %s\n", a.c_str());
            return usage();
        } else {
            pos.push_back(a);
        }
    }
    if (pos.size() != 2) return usage();
    const std::string levelFile = resolve(pos[0], dataDir);
    const std::string replayFile = resolve(pos[1], dataDir);

    std::vector<std::string> errors;
    auto level = gd::loadLevelFromFile(levelFile, errors);
    if (!level) {
        std::fprintf(stderr, "%s: level failed to load\n", levelFile.c_str());
        for (const auto& e : errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return 2;
    }
    errors.clear();
    auto replay = gd::loadReplayFromFile(replayFile, errors);
    if (!replay) {
        std::fprintf(stderr, "%s: replay failed to parse\n", replayFile.c_str());
        for (const auto& e : errors) std::fprintf(stderr, "  %s: %s\n", replayFile.c_str(), e.c_str());
        return 2;
    }
    if (replay->level != level->id) {
        std::fprintf(stderr, "%s: replay is for level %d but level file has id %d\n",
                     replayFile.c_str(), replay->level, level->id);
        return 2;
    }

    const gd::ReplayResult r = gd::runReplay(*level, *replay);
    const char* verdict = r.won ? "WIN" : (r.died ? "DEAD" : "TIMEOUT");
    std::printf("%s level=%d tick=%d end=%d x=%.3f endX=%.3f\n", verdict, level->id, r.tick,
                replay->endTick, r.x, level->endX);
    return r.won ? 0 : 1;
}
