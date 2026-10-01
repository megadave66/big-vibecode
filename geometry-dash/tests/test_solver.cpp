// Solver tests on synthetic levels built in C++.
#include <doctest/doctest.h>

#include "core/Replay.h"
#include "core/Solver.h"
#include "test_sim_helpers.h"

using namespace gd;
using simtest::block;
using simtest::makeLevel;
using simtest::spike;

namespace {

SolverOptions fastOpts() {
    SolverOptions o;
    o.maxSeconds = 30;
    return o;
}

bool replayWins(const Level& L, const SolverResult& r) {
    const Replay rep = solutionReplay(L, r, 24);
    std::vector<std::string> errors;
    auto back = parseReplay(writeReplay(rep), errors);
    REQUIRE(back.has_value());
    return runReplay(L, *back).won;
}

Level spikeLevel() {
    return makeLevel(120, {spike(15), spike(30), spike(31), spike(45), spike(46), spike(47),
                           block(60, 0, 3, 1), spike(63), spike(64), spike(80), spike(81), spike(82),
                           spike(100)});
}

}  // namespace

TEST_CASE("solver: flat level needs zero presses") {
    const Level L = makeLevel(100);
    const SolverResult r = solveLevel(L, fastOpts());
    REQUIRE(r.solved);
    CHECK(r.presses == 0);
    CHECK(r.minSlack == -1);
    CHECK(replayWins(L, r));
    CHECK(solutionReplay(L, r).inputs.empty());
}

TEST_CASE("solver: spike row level needs presses and replays to a win") {
    const Level L = spikeLevel();
    const SolverResult r = solveLevel(L, fastOpts());
    REQUIRE(r.solved);
    CHECK(r.presses >= 6);
    CHECK(r.minSlack >= 1);
    CHECK(static_cast<int>(r.held.size()) == r.winTick);
    CHECK(replayWins(L, r));
    // An empty script dies on the first spike.
    Replay none;
    none.level = L.id;
    none.tickRate = phys::kTickRate;
    none.endTick = r.winTick + 24;
    CHECK_FALSE(runReplay(L, none).won);
}

TEST_CASE("solver: ship corridor solved") {
    // Floor pillars, ceiling pillars and a narrow window; every gap >= kShipMinGap.
    const Level L = makeLevel(
        110,
        {block(20, 0, 3, 5), block(35, 6, 3, 6), block(50, 0, 3, 4), block(50, 7, 3, 5),
         block(65, 0, 4, 7), block(80, 3.5, 4, 8.5), block(95, 0, 2, 6)},
        GameMode::Ship);
    const SolverResult r = solveLevel(L, fastOpts());
    REQUIRE(r.solved);
    CHECK(r.presses >= 1);
    CHECK(replayWins(L, r));
}

TEST_CASE("solver: impossible level returns no solution") {
    // 4-high wall right after the 8-block start clear zone: above the cube's climb.
    const Level L = makeLevel(60, {block(phys::kStartClearBlocks, 0, 1, 4)});
    const SolverResult r = solveLevel(L, fastOpts());
    CHECK_FALSE(r.solved);
    CHECK_FALSE(r.timedOut);
    CHECK(r.held.empty());
    CHECK(r.furthestX < phys::kStartClearBlocks);
    CHECK(r.furthestX > phys::kStartClearBlocks - 2);
    CHECK(r.furthestTick > 0);
    CHECK(r.nodes > 0);
}

TEST_CASE("solver: deterministic output") {
    const Level L = spikeLevel();
    const SolverResult a = solveLevel(L, fastOpts());
    const SolverResult b = solveLevel(L, fastOpts());
    REQUIRE(a.solved);
    REQUIRE(b.solved);
    CHECK(a.held == b.held);
    CHECK(a.nodes == b.nodes);
    CHECK(a.minSlack == b.minSlack);
    CHECK(writeReplay(solutionReplay(L, a)) == writeReplay(solutionReplay(L, b)));
}
