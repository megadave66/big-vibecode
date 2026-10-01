// gd_solve <level.json> -o <out.replay> [--data DIR] [--max-seconds N] [--no-polish]
// Searches for an input sequence that beats the level, writes it as a replay, then re-runs the
// written file with runReplay. Exit 0 only if that re-run wins. Exit 1 = no solution found
// (prints the furthest x and tick reached), 2 = bad arguments or files.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"
#include "core/Replay.h"
#include "core/Solver.h"

namespace {

int usage() {
    std::fprintf(stderr,
                 "usage: gd_solve <level.json> -o <out.replay> [--data DIR] [--max-seconds N] "
                 "[--no-polish]\n");
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
    std::string out;
    gd::SolverOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data" || a == "-o" || a == "--max-seconds") {
            if (i + 1 >= argc) return usage();
            const std::string v = argv[++i];
            if (a == "--data") dataDir = v;
            else if (a == "-o") out = v;
            else {
                char* end = nullptr;
                const double s = std::strtod(v.c_str(), &end);
                if (end == v.c_str() || *end != '\0' || !(s > 0)) {
                    std::fprintf(stderr, "--max-seconds needs a positive number\n");
                    return usage();
                }
                opts.maxSeconds = s;
            }
        } else if (a == "--no-polish") {
            opts.polish = false;
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
    if (pos.size() != 1 || out.empty()) return usage();
    const std::string levelFile = resolve(pos[0], dataDir);

    std::vector<std::string> errors;
    auto level = gd::loadLevelFromFile(levelFile, errors);
    if (!level) {
        std::fprintf(stderr, "%s: level failed to load\n", levelFile.c_str());
        for (const auto& e : errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return 2;
    }

    const gd::SolverResult r = gd::solveLevel(*level, opts);
    if (!r.solved) {
        std::printf("NO SOLUTION level=%d furthest_x=%.3f tick=%d (%.2f s) endX=%.3f nodes=%lld "
                    "seconds=%.2f%s\n",
                    level->id, r.furthestX, r.furthestTick,
                    static_cast<double>(r.furthestTick) / gd::phys::kTickRate, level->endX, r.nodes,
                    r.seconds, r.timedOut ? " (time limit hit)" : "");
        return 1;
    }

    const gd::Replay rep = gd::solutionReplay(*level, r, opts.endMargin);
    if (!gd::saveReplayToFile(rep, out)) {
        std::fprintf(stderr, "%s: cannot write replay\n", out.c_str());
        return 2;
    }

    // Re-verify from the file on disk, exactly as gd_replay does.
    errors.clear();
    auto back = gd::loadReplayFromFile(out, errors);
    if (!back) {
        std::fprintf(stderr, "%s: written replay failed to parse\n", out.c_str());
        for (const auto& e : errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return 1;
    }
    const gd::ReplayResult v = gd::runReplay(*level, *back);

    std::string slack = r.minSlack < 0 ? std::string("n/a") : std::to_string(r.minSlack);
    if (r.minSlack >= opts.slackCap) slack += "+";
    if (r.slackPartial) slack += " (partial)";
    std::printf("%s level=%d presses=%d win_tick=%d end=%d nodes=%lld steps=%lld pass=%d "
                "min_slack=%s seconds=%.2f -> %s\n",
                v.won ? "SOLVED" : "VERIFY FAILED", level->id, r.presses, r.winTick, back->endTick,
                r.nodes, r.steps, r.pass, slack.c_str(), r.seconds, out.c_str());
    return v.won ? 0 : 1;
}
