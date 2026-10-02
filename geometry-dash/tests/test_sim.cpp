// Physics: cube basics, collisions, hazards, determinism, snapshots.
#include <doctest/doctest.h>

#include <cmath>

#include "core/FixedTimestep.h"
#include "test_sim_helpers.h"

using namespace gd;
using namespace simtest;
namespace P = gd::phys;

namespace {

struct JumpMeasure {
    double apex = 0;
    int airTicks = 0;
    double length = 0;
};

// Tap at tick 0 on flat ground, measure the jump.
JumpMeasure measureJump(Speed speed) {
    Level L = makeLevel(200, {}, GameMode::Cube, speed);
    Sim sim(L);
    JumpMeasure m;
    const double x0 = sim.player().x;
    sim.step(true);
    REQUIRE(sim.events().has(SimEventType::Jump));
    int t = 1;
    while (!sim.player().grounded && t < 1000) {
        m.apex = std::max(m.apex, sim.player().y);
        sim.step(false);
        ++t;
    }
    REQUIRE(sim.player().grounded);
    m.airTicks = sim.tick();
    m.length = sim.player().x - x0;
    return m;
}

}  // namespace

TEST_CASE("flat run reaches the end wall at the predicted tick") {
    for (Speed sp : {Speed::Slow, Speed::Normal, Speed::Fast, Speed::Faster}) {
        Level L = makeLevel(100, {}, GameMode::Cube, sp);
        Sim sim(L);
        const double v = speedValue(sp);
        const int predicted = static_cast<int>(std::ceil((100.0 - P::kPlayerSize) / (v * P::kDt)));
        run(sim, 100000, [](const Sim&) { return false; });
        CHECK(sim.won());
        CHECK_FALSE(sim.dead());
        CHECK(sim.tick() == predicted);
        CHECK(sim.progress() == 1.0);
        CHECK(sim.player().y == 0.0);
        CHECK(sim.player().grounded);
    }
}

TEST_CASE("progress grows from 0 to 1") {
    Level L = makeLevel(50);
    Sim sim(L);
    CHECK(sim.progress() == 0.0);
    for (int i = 0; i < 240; ++i) sim.step(false);
    CHECK(sim.progress() == doctest::Approx(sim.player().x / 49.0));
    CHECK(sim.progress() > 0.1);
    CHECK(sim.progress() < 1.0);
}

TEST_CASE("cube jump apex and air time match the derived constants") {
    const JumpMeasure m = measureJump(Speed::Normal);
    CHECK(std::abs(m.apex - P::kCubeJumpApex) <= P::kCubeJumpVelocity * P::kDt);
    CHECK(std::abs(m.airTicks * P::kDt - P::kCubeAirTime) <= P::kDt);
    // GD-like: about 2.2 blocks high.
    CHECK(P::kCubeJumpApex > 2.1);
    CHECK(P::kCubeJumpApex < 2.4);
}

TEST_CASE("cube jump length matches cubeJumpLength at every speed") {
    for (Speed sp : {Speed::Slow, Speed::Normal, Speed::Fast, Speed::Faster}) {
        const JumpMeasure m = measureJump(sp);
        const double v = speedValue(sp);
        CHECK(std::abs(m.length - P::cubeJumpLength(v)) <= 1.5 * v * P::kDt);
    }
}

TEST_CASE("holding re-jumps on the tick after landing") {
    Level L = makeLevel(200);
    Sim sim(L);
    int jumps = 0, lands = 0, lastLand = -1;
    for (int t = 0; t < 600; ++t) {
        sim.step(true);
        if (sim.events().has(SimEventType::Jump)) {
            ++jumps;
            if (lastLand >= 0) CHECK(t == lastLand + 1);
        }
        if (sim.events().has(SimEventType::Land)) {
            ++lands;
            lastLand = t;
        }
    }
    CHECK(jumps >= 5);
    CHECK(lands >= 4);
    CHECK_FALSE(sim.dead());
}

TEST_CASE("a press shortly before landing is buffered; an early press is not") {
    // Find the landing tick of a tap-at-0 jump.
    const int land = measureJump(Speed::Normal).airTicks;  // grounded after `land` steps
    Level L = makeLevel(200);

    SUBCASE("buffered press (released before landing) jumps on landing") {
        Sim sim(L);
        const int pressTick = land - 12;  // 0.05 s before landing
        int jumps = 0;
        for (int t = 0; t < land + 30; ++t) {
            sim.step(t == 0 || t == pressTick);
            if (sim.events().has(SimEventType::Jump)) ++jumps;
        }
        CHECK(jumps == 2);
    }
    SUBCASE("press long before landing does not jump") {
        Sim sim(L);
        const int pressTick = land - 2 * static_cast<int>(P::kJumpBufferTime * P::kTickRate);
        int jumps = 0;
        for (int t = 0; t < land + 30; ++t) {
            sim.step(t == 0 || t == pressTick);
            if (sim.events().has(SimEventType::Jump)) ++jumps;
        }
        CHECK(jumps == 1);
    }
}

TEST_CASE("cube lands on a block top and runs along it") {
    Level L = makeLevel(60, {block(15, 0, 20, 1)});
    // Search for a jump tick that lands on the block; there must be one.
    int found = -1;
    for (int jt = 0; jt < 400 && found < 0; ++jt) {
        Sim sim(L);
        bool onTop = false;
        while (!sim.dead() && !sim.won() && sim.tick() < 2000) {
            sim.step(sim.tick() == jt);
            if (sim.player().grounded && sim.player().y == 1.0) onTop = true;
        }
        if (sim.won() && onTop) found = jt;
    }
    REQUIRE(found >= 0);
    Sim sim(L);
    run(sim, 2000, tapAt(found));
    CHECK(sim.won());
    // Ran off the far end of the block back onto the ground.
    CHECK(sim.player().y == 0.0);
}

TEST_CASE("running into a block side kills the cube where the inner box meets it") {
    Level L = makeLevel(60, {block(20, 0, 2, 1)});
    Sim sim(L);
    run(sim, 5000, [](const Sim&) { return false; });
    REQUIRE(sim.dead());
    CHECK(sim.events().has(SimEventType::Death));
    const double innerRight = sim.player().x + (P::kPlayerSize + P::kInnerHitbox) / 2;
    CHECK(innerRight > 20.0);
    CHECK(innerRight - 20.0 <= P::kSpeedNormal * P::kDt + 1e-9);
    // The outer box had already overlapped the block (side hits are forgiven until then).
    CHECK(sim.player().x + P::kPlayerSize > 20.3);
}

TEST_CASE("cube head hit on a block underside kills; walking under it is safe") {
    // Overhead slab with its underside 1.5 above the ground.
    Level L = makeLevel(60, {block(10, 1.5, 20, 1)});
    SUBCASE("walk under") {
        Sim sim(L);
        run(sim, 5000, [](const Sim&) { return false; });
        CHECK(sim.won());
    }
    SUBCASE("jump into it") {
        Sim sim(L);
        // Jump while under the slab.
        run(sim, 5000, [](const Sim& s) { return s.player().x > 15.0; });
        CHECK(sim.dead());
        CHECK(sim.player().x > 15.0);
        CHECK(sim.player().x < 30.0);
    }
}

TEST_CASE("spike hitboxes: values, death, and a near miss that must survive") {
    const Aabb up = Sim::spikeHitbox(spike(10, 0, SpikeDir::Up));
    CHECK(up.x == doctest::Approx(10 + P::kSpikeHitX));
    CHECK(up.y == doctest::Approx(P::kSpikeHitY));
    CHECK(up.w == doctest::Approx(P::kSpikeHitW));
    CHECK(up.h == doctest::Approx(P::kSpikeHitH));
    const Aabb dn = Sim::spikeHitbox(spike(10, 3, SpikeDir::Down));
    CHECK(dn.top() == doctest::Approx(4.0 - P::kSpikeHitY));
    CHECK(dn.y == doctest::Approx(4.0 - P::kSpikeHitY - P::kSpikeHitH));

    Level L = makeLevel(40, {spike(15)});
    SUBCASE("running into a spike kills") {
        Sim sim(L);
        run(sim, 5000, [](const Sim&) { return false; });
        CHECK(sim.dead());
        CHECK(sim.player().x + P::kPlayerSize > 15 + P::kSpikeHitX);
        CHECK(sim.player().x < 15 + P::kSpikeHitX + P::kSpikeHitW);
    }
    SUBCASE("near miss: outer box enters the spike cell but not the hitbox") {
        const Aabb cell{15, 0, 1, 1};
        bool foundNearMiss = false, foundDeath = false;
        for (int jt = 0; jt < 400; ++jt) {
            Sim sim(L);
            bool enteredCell = false;
            while (!sim.dead() && !sim.won() && sim.tick() < 3000) {
                sim.step(sim.tick() == jt);
                if (overlaps(Sim::outerBox(sim.player()), cell)) enteredCell = true;
            }
            if (sim.won() && enteredCell) foundNearMiss = true;
            if (sim.dead()) foundDeath = true;
        }
        CHECK(foundNearMiss);
        CHECK(foundDeath);
    }
}

TEST_CASE("dir=down spike hangs from above: safe to walk under, deadly to jump into") {
    // Cell y 1..2; hitbox 1.45..2.0, above a walking cube (top at 1.0).
    Level L = makeLevel(40, {spike(15, 1, SpikeDir::Down)});
    SUBCASE("walk under") {
        Sim sim(L);
        run(sim, 5000, [](const Sim&) { return false; });
        CHECK(sim.won());
    }
    SUBCASE("jump into it") {
        Sim sim(L);
        run(sim, 5000, [](const Sim& s) { return s.player().x > 14.0 && s.player().x < 14.2; });
        CHECK(sim.dead());
    }
    SUBCASE("dir=up spike in the same cell would not be a ceiling hazard") {
        // Same cell, pointing up: hitbox 1.0..1.55 starts at the cube's top edge -> touching only.
        Level L2 = makeLevel(40, {spike(15, 1, SpikeDir::Up)});
        Sim sim(L2);
        run(sim, 5000, [](const Sim&) { return false; });
        CHECK(sim.won());
    }
}

TEST_CASE("no tunnelling: max fall speed onto a thin platform at faster speed") {
    // Ship climbs high, a mode portal turns it into a cube that free-falls onto a thin platform.
    Level L = makeLevel(120,
                        {modePortal(30, GameMode::Cube), platform(40, 2, 70)},
                        GameMode::Ship, Speed::Faster, Gravity::Down, 20);
    Sim sim(L);
    double minVy = 0;
    bool landedOnPlatform = false;
    run(sim, 20000, [&](const Sim& s) {
        minVy = std::min(minVy, s.player().vy);
        if (s.player().mode == GameMode::Cube && s.player().grounded &&
            s.player().y == 2.0 + P::kPlatformHeight)
            landedOnPlatform = true;
        return s.player().mode == GameMode::Ship && s.player().y < 16.0;
    });
    CHECK(minVy == doctest::Approx(-P::kCubeMaxFallSpeed));
    CHECK(landedOnPlatform);
    CHECK(sim.won());
}

TEST_CASE("no tunnelling: a thin wall stops the cube at faster speed") {
    Level L = makeLevel(60, {block(20, 0, 0.5, 2)}, GameMode::Cube, Speed::Faster);
    Sim sim(L);
    run(sim, 5000, [](const Sim&) { return false; });
    CHECK(sim.dead());
    CHECK(sim.player().x < 20.5);
}

TEST_CASE("cube rotation spins in the air and snaps to 90 degrees on landing") {
    Level L = makeLevel(100);
    Sim sim(L);
    sim.step(true);
    for (int i = 0; i < 20; ++i) sim.step(false);
    const double r = sim.player().rotation;
    CHECK(std::fmod(r, 90.0) != 0.0);
    while (!sim.player().grounded) sim.step(false);
    CHECK(std::fmod(sim.player().rotation, 90.0) == 0.0);
}

TEST_CASE("events: jump, land, win") {
    Level L = makeLevel(30);
    Sim sim(L);
    sim.step(true);
    CHECK(sim.events().has(SimEventType::Jump));
    sim.step(false);
    CHECK(sim.events().empty());
    bool landed = false, won = false;
    while (!sim.won()) {
        sim.step(false);
        landed |= sim.events().has(SimEventType::Land);
        won |= sim.events().has(SimEventType::Win);
    }
    CHECK(landed);
    CHECK(won);
    sim.step(false);  // after win: nothing happens
    CHECK(sim.events().empty());
}

namespace {
Level busyLevel() {
    std::vector<Object> o;
    for (int i = 0; i < 30; ++i) {
        o.push_back(block(12 + i * 9, 0, 2, 1 + (i % 2) * 0.5));
        o.push_back(platform(16 + i * 9, 2.5, 2));
    }
    o.push_back(gravityPortal(150, Gravity::Up));
    o.push_back(gravityPortal(170, Gravity::Down));
    o.push_back(modePortal(200, GameMode::Ship));
    o.push_back(speedPortal(210, Speed::Fast));
    o.push_back(modePortal(240, GameMode::Cube));
    return makeLevel(300, o);
}
}  // namespace

TEST_CASE("determinism: two runs with the same script are bit-identical") {
    Level L = busyLevel();
    const auto script = lcgScript(20000, 1234);
    Sim a(L), b(L);
    a.setInvincible(true);
    b.setInvincible(true);
    for (int t = 0; t < 20000 && !a.won(); ++t) {
        a.step(script[t]);
        b.step(script[t]);
        REQUIRE(sameState(a.player(), b.player()));
    }
    CHECK(a.won());
}

TEST_CASE("frame-rate independence: 30/60/144/240 fps give the same final state") {
    Level L = busyLevel();
    const int N = 3000;
    const auto script = lcgScript(N, 99);
    std::vector<PlayerState> finals;
    for (double fps : {30.0, 60.0, 144.0, 240.0}) {
        Sim sim(L);
        sim.setInvincible(true);
        FixedTimestep ft;
        int frames = 0;
        while (sim.tick() < N && !sim.won()) {
            const int n = ft.advance(1.0 / fps);
            for (int k = 0; k < n && sim.tick() < N && !sim.won(); ++k) sim.step(script[sim.tick()]);
            ++frames;
        }
        CHECK(frames >= static_cast<int>(N / P::kTickRate * fps) - 2);
        finals.push_back(sim.player());
    }
    for (size_t i = 1; i < finals.size(); ++i) CHECK(sameState(finals[0], finals[i]));
}

TEST_CASE("snapshot/restore and Sim copies reproduce the run exactly") {
    Level L = busyLevel();
    const auto script = lcgScript(4000, 7);
    Sim sim(L);
    sim.setInvincible(true);
    for (int t = 0; t < 1000; ++t) sim.step(script[t]);
    const Snapshot snap = sim.snapshot();
    Sim copy = sim;
    for (int t = 1000; t < 3000; ++t) sim.step(script[t]);
    const PlayerState after = sim.player();

    sim.restore(snap);
    CHECK(sim.tick() == 1000);
    CHECK(sim.events().empty());
    for (int t = 1000; t < 3000; ++t) sim.step(script[t]);
    CHECK(sameState(sim.player(), after));

    for (int t = 1000; t < 3000; ++t) copy.step(script[t]);
    CHECK(sameState(copy.player(), after));

    sim.reset();
    CHECK(sim.tick() == 0);
    CHECK(sim.player().x == 0.0);
    CHECK(sim.player().grounded);
}

TEST_CASE("invincible (fly mode) reaches the end of a deadly level") {
    std::vector<Object> o;
    for (int i = 10; i < 80; ++i) o.push_back(spike(i));
    o.push_back(block(40, 0, 1, 6));
    o.push_back(block(60, 3, 3, 1));
    Level L = makeLevel(100, o);
    Sim normal(L);
    run(normal, 10000, [](const Sim&) { return false; });
    CHECK(normal.dead());

    Sim fly(L);
    fly.setInvincible(true);
    bool anyDeath = false;
    while (!fly.won() && fly.tick() < 10000) {
        fly.step(fly.tick() % 50 == 0);
        anyDeath |= fly.events().has(SimEventType::Death);
    }
    CHECK(fly.won());
    CHECK_FALSE(fly.dead());
    CHECK_FALSE(anyDeath);
}

TEST_CASE("large level: sim handles tens of thousands of objects") {
    std::vector<Object> o;
    for (int i = 0; i < 40000; ++i) o.push_back(block(10 + i * 0.5, 8 + (i % 3), 0.5, 0.5));
    Level L = makeLevel(20030, o);
    Sim sim(L);
    run(sim, 1000000, [](const Sim&) { return false; });
    CHECK(sim.won());
}
