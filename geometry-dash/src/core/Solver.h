#pragma once
// Automatic input search that beats a level. Owner: solver section.
//
// Algorithm (details in Solver.cpp):
//   1. Search: depth-first search over coarse decisions (hold or release for K ticks), with
//      backtracking via Sim::snapshot()/restore() and a visited set on a discretized state
//      (tick, y bucket, vy bucket, mode, gravity, grounded, held, buffer). Cube nodes try
//      "release" first (fewest presses); ship nodes try "keep the current input" first.
//      Passes run from coarse to fine (K = 6, 4, 2, 1 with finer buckets) until one succeeds.
//   2. Polish: drop presses that are not needed, then shift each hold to the middle of the
//      window of shifts that still win (human-plausible, not frame-perfect).
//   3. Slack: for each hold, the largest d such that moving the hold by any offset in
//      [-d, +d] ticks (others fixed) still wins. minSlack = smallest over all holds.
// x depends only on the tick (speed portals fire on x), so every winning run wins on the same
// tick. All budgets are node or step counts, so results do not depend on machine speed;
// only maxSeconds (a hard abort) uses the clock.

#include <cstdint>
#include <string>
#include <vector>

#include "core/Level.h"
#include "core/Replay.h"

namespace gd {

struct SolverOptions {
    double maxSeconds = 60.0;  // hard wall-clock abort for the whole solve
    bool polish = true;        // remove needless presses and centre holds in their windows
    bool measureSlack = true;  // compute minSlack after polish
    int slackCap = 16;         // largest shift (ticks) tried per hold when centring / measuring
    int endMargin = 24;        // replay end tick = win tick + this
};

struct SolverResult {
    bool solved = false;
    bool timedOut = false;     // maxSeconds hit before a solution was found
    std::vector<bool> held;    // held state per tick, 0 .. winTick-1
    int winTick = 0;           // ticks stepped when the end wall is reached
    int presses = 0;
    int minSlack = -1;         // -1 = not measured (or no presses); capped at slackCap
    bool slackPartial = false; // step budget ran out; minSlack covers only some holds
    double furthestX = 0;      // furthest x any searched run reached
    int furthestTick = 0;      // tick at which furthestX was reached
    long long nodes = 0;       // search nodes expanded (all passes)
    long long steps = 0;       // sim ticks stepped (search + polish)
    int pass = 0;              // 1-based search pass that found the solution
    double seconds = 0;
};

SolverResult solveLevel(const Level& level, const SolverOptions& opts = {});

// Replay for a solved result: level id, tickrate, press/release lines, end = winTick + margin.
Replay solutionReplay(const Level& level, const SolverResult& result, int endMargin = 24);

}  // namespace gd
