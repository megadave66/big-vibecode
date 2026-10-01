// Practice mode checkpoints.
#include <doctest/doctest.h>

#include "core/Practice.h"
#include "test_sim_helpers.h"

using namespace gd;
using namespace simtest;

TEST_CASE("practice: no checkpoint -> respawn resets (normal-mode restart)") {
    Level L = makeLevel(60, {block(20, 0, 1, 1)});
    Sim sim(L);
    Practice pr;
    CHECK(pr.count() == 0);
    run(sim, 5000, [](const Sim&) { return false; });
    REQUIRE(sim.dead());
    pr.respawn(sim);
    CHECK_FALSE(sim.dead());
    CHECK(sim.tick() == 0);
    CHECK(sim.player().x == 0.0);
}

TEST_CASE("practice: add, respawn at last checkpoint, removeLast") {
    Level L = makeLevel(80, {block(30, 0, 1, 1)});
    Sim sim(L);
    Practice pr;
    for (int i = 0; i < 240; ++i) sim.step(false);
    CHECK(pr.add(sim));
    const PlayerState cp1 = sim.player();
    for (int i = 0; i < 240; ++i) sim.step(false);
    CHECK(pr.add(sim));
    const PlayerState cp2 = sim.player();
    CHECK(pr.count() == 2);

    run(sim, 5000, [](const Sim&) { return false; });
    REQUIRE(sim.dead());
    CHECK_FALSE(pr.add(sim));  // only while alive
    CHECK(pr.count() == 2);

    pr.respawn(sim);
    CHECK_FALSE(sim.dead());
    CHECK(sameState(sim.player(), cp2));

    // Respawning twice from the same checkpoint gives the same state.
    run(sim, 5000, [](const Sim&) { return false; });
    pr.respawn(sim);
    CHECK(sameState(sim.player(), cp2));

    pr.removeLast();
    CHECK(pr.count() == 1);
    pr.respawn(sim);
    CHECK(sameState(sim.player(), cp1));

    pr.removeLast();
    pr.removeLast();  // no-op when empty
    CHECK(pr.count() == 0);
    pr.respawn(sim);
    CHECK(sim.tick() == 0);
}

TEST_CASE("practice: a checkpoint taken mid-air keeps velocity and mode") {
    Level L = makeLevel(80, {modePortal(10, GameMode::Ship)});
    Sim sim(L);
    while (sim.player().mode != GameMode::Ship) sim.step(false);
    for (int i = 0; i < 30; ++i) sim.step(true);
    Practice pr;
    REQUIRE(pr.add(sim));
    const PlayerState cp = sim.player();
    for (int i = 0; i < 100; ++i) sim.step(false);
    pr.respawn(sim);
    CHECK((sim.player().mode == GameMode::Ship));
    CHECK(sim.player().vy == cp.vy);
    CHECK(sim.player().nextPortal == cp.nextPortal);
    CHECK(sameState(sim.player(), cp));
}

TEST_CASE("practice: cannot add after winning") {
    Level L = makeLevel(20);
    Sim sim(L);
    run(sim, 5000, [](const Sim&) { return false; });
    REQUIRE(sim.won());
    Practice pr;
    CHECK_FALSE(pr.add(sim));
}
