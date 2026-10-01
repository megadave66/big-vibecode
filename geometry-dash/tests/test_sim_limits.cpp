// Physics: the derived authoring limits in PhysicsConstants.h hold in the real sim.
// The validator relies on these (kCubeMaxSpikeRun / cubeMaxSpikeRun, kCubeMaxClimb,
// kShipMinGap, kShipMaxSlope).
#include <doctest/doctest.h>

#include <cmath>

#include "test_sim_helpers.h"

using namespace gd;
using namespace simtest;
namespace P = gd::phys;

namespace {

// Can a cube clear `n` adjacent floor spikes at `speed` with one tap at some tick?
bool spikeRunClearable(int n, Speed speed) {
    std::vector<Object> o;
    for (int i = 0; i < n; ++i) o.push_back(spike(20 + i));
    Level L = makeLevel(20 + n + 8, o, GameMode::Cube, speed);
    const int lastTick = static_cast<int>(22.0 / (speedValue(speed) * P::kDt));
    for (int jt = 0; jt < lastTick; ++jt) {
        Sim sim(L);
        run(sim, 100000, tapAt(jt));
        if (sim.won()) return true;
    }
    return false;
}

// Can a cube climb onto a long block of height h with one tap?
bool climbable(double h, Speed speed) {
    Level L = makeLevel(60, {block(20, 0, 40, h)}, GameMode::Cube, speed);
    const int lastTick = static_cast<int>(22.0 / (speedValue(speed) * P::kDt));
    for (int jt = 0; jt < lastTick; ++jt) {
        Sim sim(L);
        run(sim, 100000, tapAt(jt));
        if (sim.won()) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("triple spike is clearable at normal speed, four spikes are not") {
    CHECK(P::cubeMaxSpikeRun(P::kSpeedNormal) == 3.0);
    CHECK(spikeRunClearable(3, Speed::Normal));
    CHECK_FALSE(spikeRunClearable(4, Speed::Normal));
}

TEST_CASE("cubeMaxSpikeRun(speed) is clearable at every speed and one more spike is not") {
    for (Speed sp : {Speed::Slow, Speed::Normal, Speed::Fast, Speed::Faster}) {
        const int n = static_cast<int>(P::cubeMaxSpikeRun(speedValue(sp)));
        CAPTURE(static_cast<int>(sp));
        CAPTURE(n);
        CHECK(n >= 1);
        CHECK(spikeRunClearable(n, sp));
        CHECK_FALSE(spikeRunClearable(n + 1, sp));
    }
}

TEST_CASE("kCubeMaxClimb: the highest 0.5-step block under it is climbable at every speed") {
    const double h = std::floor(P::kCubeMaxClimb * 2.0) / 2.0;
    CHECK(h >= 1.5);
    for (Speed sp : {Speed::Slow, Speed::Normal, Speed::Fast, Speed::Faster}) {
        CAPTURE(static_cast<int>(sp));
        CHECK(climbable(h, sp));
        // Well above apex + landing tolerance is never climbable.
        CHECK_FALSE(climbable(std::ceil(P::kCubeJumpApex + P::kLandSnap) + 0.5, sp));
    }
}

namespace {

// Zig-zag ship corridor: free gap `gap`, centre moving `slope` blocks per block of x.
struct Corridor {
    double gap, slope;
    int x0 = 10, cols = 96;
    double floorAt(int col) const {
        const int i = col - x0;
        if (i < 0 || i >= cols) return 0.0;
        const int period = static_cast<int>(std::lround(8.0 / slope)) * 2;  // rise 8 then fall 8
        const int k = i % period;
        const int half = period / 2;
        return slope * (k < half ? k : period - k);
    }
    double centreAt(int col) const { return floorAt(col) + gap * 0.5; }
    std::vector<Object> objects(double ceiling) const {
        std::vector<Object> o;
        for (int i = 0; i < cols; ++i) {
            const double f = floorAt(x0 + i);
            if (f > 0) o.push_back(block(x0 + i, 0, 1, f));
            o.push_back(block(x0 + i, f + gap, 1, ceiling - (f + gap)));
        }
        return o;
    }
};

// Simple look-ahead bang-bang controller: aim the ship centre at the corridor centre a little
// ahead, using a velocity-predicted position.
bool flyCorridor(const Corridor& c, Speed sp) {
    const double ceiling = 12;
    Level L = makeLevel(c.x0 + c.cols + 10, c.objects(ceiling), GameMode::Ship, sp, Gravity::Down,
                        ceiling);
    Sim sim(L);
    const double v = speedValue(sp);
    run(sim, 100000, [&](const Sim& s) {
        const PlayerState& p = s.player();
        const double look = v * 0.2;
        double sum = 0;
        int n = 0;
        for (double x = p.x; x <= p.x + P::kPlayerSize + look; x += 0.25, ++n)
            sum += c.centreAt(static_cast<int>(std::floor(x)));
        const double target = sum / n;
        return p.y + 0.5 + p.vy * 0.1 < target;
    });
    return sim.won();
}

}  // namespace

TEST_CASE("ship: a kShipMinGap corridor with kShipMaxSlope is flyable at every speed") {
    const Corridor c{P::kShipMinGap, P::kShipMaxSlope};
    for (Speed sp : {Speed::Slow, Speed::Normal, Speed::Fast, Speed::Faster}) {
        CAPTURE(static_cast<int>(sp));
        CHECK(flyCorridor(c, sp));
    }
}

TEST_CASE("ship: the max slope stays inside what the ship can follow") {
    // Following the corridor at kShipMaxSlope needs vertical speed slope*speed < kShipMaxVy.
    CHECK(P::kShipMaxSlope * P::kSpeedFaster < P::kShipMaxVy);
    // And the corridor leaves room for the player box.
    CHECK(P::kShipMinGap > P::kPlayerSize + 1.0);
}
