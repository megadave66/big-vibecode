// Physics: portals (gravity, speed, mode) and the ship.
#include <doctest/doctest.h>

#include <cmath>

#include "test_sim_helpers.h"

using namespace gd;
using namespace simtest;
namespace P = gd::phys;

TEST_CASE("gravity portal: cube falls up and lands on the ceiling") {
    Level L = makeLevel(60, {gravityPortal(5, Gravity::Up)});
    Sim sim(L);
    int portalTick = -1;
    run(sim, 10000, [&](const Sim& s) {
        if (s.events().has(SimEventType::Portal) && portalTick < 0) portalTick = s.tick() - 1;
        return false;
    });
    CHECK(sim.won());
    CHECK(portalTick >= 0);
    CHECK(sim.player().gravity == Gravity::Up);
    CHECK(sim.player().grounded);
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
}

TEST_CASE("gravity up: cube stands on block bottoms, jumps downward, then ceiling") {
    Level L = makeLevel(60, {gravityPortal(2, Gravity::Up), block(0, 9, 30, 3)});
    Sim sim(L);
    while (sim.player().x < 20) sim.step(false);
    REQUIRE_FALSE(sim.dead());
    CHECK(sim.player().grounded);
    CHECK(sim.player().y == 9.0 - P::kPlayerSize);
    // Jump: goes toward the ground.
    sim.step(true);
    CHECK(sim.events().has(SimEventType::Jump));
    double minY = sim.player().y;
    while (!sim.player().grounded) {
        sim.step(false);
        minY = std::min(minY, sim.player().y);
    }
    CHECK(8.0 - minY == doctest::Approx(P::kCubeJumpApex).epsilon(0.05));
    // Past the block: rises to the ceiling.
    run(sim, 10000, [](const Sim&) { return false; });
    CHECK(sim.won());
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
}

TEST_CASE("gravity up: a block side still kills") {
    Level L = makeLevel(60, {gravityPortal(2, Gravity::Up), block(25, 10, 2, 2)});
    Sim sim(L);
    run(sim, 10000, [](const Sim&) { return false; });
    CHECK(sim.dead());
}

TEST_CASE("speed portal changes speed exactly at the trigger") {
    Level L = makeLevel(80, {speedPortal(20, Speed::Fast)});
    Sim sim(L);
    double prevX = 0;
    while (!sim.events().has(SimEventType::Portal)) {
        prevX = sim.player().x;
        CHECK(sim.player().speed == Speed::Normal);
        sim.step(false);
        REQUIRE(sim.tick() < 5000);
    }
    const double trigger = 20.5;
    CHECK(prevX + 0.5 < trigger);
    CHECK(sim.player().x + 0.5 >= trigger);
    CHECK((sim.events()[0].portal == ObjType::PortalSpeed));
    CHECK(sim.player().speed == Speed::Fast);
    CHECK(sim.player().vx == P::kSpeedFast);
    // This tick moved at the old speed; the next one at the new speed.
    CHECK(sim.player().x - prevX == doctest::Approx(P::kSpeedNormal * P::kDt));
    const double x0 = sim.player().x;
    sim.step(false);
    CHECK(sim.player().x - x0 == doctest::Approx(P::kSpeedFast * P::kDt));
    // Fires once only.
    run(sim, 10000, [](const Sim& s) {
        CHECK_FALSE(s.events().has(SimEventType::Portal));
        return false;
    });
    CHECK(sim.won());
}

TEST_CASE("portals fire at any height (column triggers)") {
    Level L = makeLevel(60, {speedPortal(20, Speed::Slow)}, GameMode::Ship, Speed::Normal,
                        Gravity::Down, 12);
    Sim sim(L);
    run(sim, 10000, [](const Sim& s) { return s.player().y < 9.0; });  // fly high, above the drawing
    CHECK(sim.won());
    CHECK(sim.player().speed == Speed::Slow);
}

TEST_CASE("mode portals: cube -> ship -> cube") {
    Level L = makeLevel(90, {modePortal(10, GameMode::Ship), modePortal(40, GameMode::Cube)});
    Sim sim(L);
    double maxShipY = 0;
    bool sawShip = false;
    run(sim, 20000, [&](const Sim& s) {
        if (s.player().mode == GameMode::Ship) {
            sawShip = true;
            maxShipY = std::max(maxShipY, s.player().y);
        }
        return s.player().x < 8 || (s.player().mode == GameMode::Ship && s.player().y < 4);
    });
    CHECK(sawShip);
    CHECK(maxShipY > 4.0);
    CHECK(sim.won());
    CHECK(sim.player().mode == GameMode::Cube);
    CHECK(sim.player().grounded);
}

TEST_CASE("mode portal keeps vertical velocity, clamped to ship limits") {
    Level L = makeLevel(60, {modePortal(10, GameMode::Ship)});
    Sim sim(L);
    // Jump so the cube hits the portal while still rising fast.
    while (sim.player().x + 0.5 < 10.5 - 3 * P::kSpeedNormal * P::kDt) sim.step(false);
    sim.step(true);
    CHECK(sim.player().vy > P::kShipMaxVy);
    while (!sim.events().has(SimEventType::Portal)) sim.step(false);
    CHECK(sim.player().mode == GameMode::Ship);
    CHECK(sim.player().vy == P::kShipMaxVy);

    // Ship -> cube keeps the ship's velocity unchanged.
    Level L2 = makeLevel(60, {modePortal(3, GameMode::Cube)}, GameMode::Ship);
    Sim s2(L2);
    double vyBefore = 0;
    while (!s2.events().has(SimEventType::Portal)) {
        s2.step(true);
        if (!s2.events().has(SimEventType::Portal)) vyBefore = s2.player().vy;
    }
    CHECK(s2.player().mode == GameMode::Cube);
    CHECK(s2.player().vy == doctest::Approx(std::min(vyBefore + P::kShipLift * P::kDt, P::kShipMaxVy)));
}

TEST_CASE("ship: vertical speed is clamped both ways") {
    Level L = makeLevel(200, {}, GameMode::Ship, Speed::Normal, Gravity::Down, 20);
    Sim sim(L);
    CHECK(sim.player().y == P::kShipStartY);
    for (int i = 0; i < 80; ++i) sim.step(true);
    CHECK(sim.player().vy == P::kShipMaxVy);
    for (int i = 0; i < 125; ++i) sim.step(false);
    CHECK(sim.player().vy == -P::kShipMaxVy);
    CHECK_FALSE(sim.dead());
    // Ship tilts with vy.
    CHECK(sim.player().rotation > 270.0);
    for (int i = 0; i < 100; ++i) sim.step(true);
    CHECK(sim.player().rotation > 0.0);
    CHECK(sim.player().rotation < 90.0);
}

TEST_CASE("ship slides along the world ceiling and ground") {
    Level L = makeLevel(200, {}, GameMode::Ship);
    Sim sim(L);
    for (int i = 0; i < 600; ++i) sim.step(true);
    CHECK_FALSE(sim.dead());
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
    CHECK(sim.player().vy == 0.0);
    for (int i = 0; i < 600; ++i) sim.step(false);
    CHECK_FALSE(sim.dead());
    CHECK(sim.player().y == 0.0);
    CHECK(sim.player().grounded);
    CHECK(sim.player().vy == 0.0);
}

TEST_CASE("ship slides on block tops and bottoms inside a tunnel") {
    // Tunnel x 20..60: floor top at 2, roof underside at 5.
    Level L = makeLevel(80, {block(20, 0, 40, 2), block(20, 5, 40, 7)}, GameMode::Ship);
    Sim sim(L);
    bool roofSlide = false, floorSlide = false;
    run(sim, 20000, [&](const Sim& s) {
        const double x = s.player().x;
        if (x > 30 && x < 34 && s.player().y == 4.0) roofSlide = true;
        if (x > 52 && x < 58 && s.player().y == 2.0 && s.player().grounded) floorSlide = true;
        if (x < 20) return s.player().y + s.player().vy * 0.1 < 3.0;
        if (x < 40) return true;
        return x >= 60 && s.player().y < 3.0;
    });
    CHECK_FALSE(sim.dead());
    CHECK(sim.won());
    CHECK(roofSlide);
    CHECK(floorSlide);
}

TEST_CASE("ship hitting a wall dies") {
    Level L = makeLevel(80, {block(20, 0, 1, 12)}, GameMode::Ship);
    Sim sim(L);
    run(sim, 20000, [](const Sim&) { return false; });
    CHECK(sim.dead());
}

TEST_CASE("ship under gravity up: holding pushes toward the ground") {
    Level L = makeLevel(80, {}, GameMode::Ship, Speed::Normal, Gravity::Up);
    Sim sim(L);
    const double y0 = sim.player().y;
    CHECK(y0 == L.ceiling - P::kPlayerSize - P::kShipStartY);
    for (int i = 0; i < 30; ++i) sim.step(true);
    CHECK(sim.player().y < y0);
    CHECK(sim.player().vy < 0.0);
    for (int i = 0; i < 400; ++i) sim.step(false);
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
    CHECK(sim.player().grounded);
}

TEST_CASE("cube starting with gravity up stands on the ceiling") {
    Level L = makeLevel(40, {}, GameMode::Cube, Speed::Normal, Gravity::Up);
    Sim sim(L);
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
    run(sim, 10000, [](const Sim&) { return false; });
    CHECK(sim.won());
    CHECK(sim.player().y == L.ceiling - P::kPlayerSize);
}
