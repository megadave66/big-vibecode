#include <doctest/doctest.h>

#include <string>

#include "core/Level.h"
#include "core/PhysicsConstants.h"

using namespace gd;

namespace {
Object speedPortal(double x, Speed s) {
    Object o;
    o.type = ObjType::PortalSpeed;
    o.x = x;
    o.speed = s;
    return o;
}
}  // namespace

TEST_CASE("speedValue matches PhysicsConstants") {
    CHECK(speedValue(Speed::Slow) == phys::kSpeedSlow);
    CHECK(speedValue(Speed::Normal) == phys::kSpeedNormal);
    CHECK(speedValue(Speed::Fast) == phys::kSpeedFast);
    CHECK(speedValue(Speed::Faster) == phys::kSpeedFaster);
    CHECK(speedValue(Speed::Slow) < speedValue(Speed::Normal));
    CHECK(speedValue(Speed::Fast) < speedValue(Speed::Faster));
}

TEST_CASE("toString gives the JSON type names") {
    CHECK(std::string(toString(ObjType::Block)) == "block");
    CHECK(std::string(toString(ObjType::Platform)) == "platform");
    CHECK(std::string(toString(ObjType::Spike)) == "spike");
    CHECK(std::string(toString(ObjType::PortalGravity)) == "portal_gravity");
    CHECK(std::string(toString(ObjType::PortalMode)) == "portal_mode");
    CHECK(std::string(toString(ObjType::PortalSpeed)) == "portal_speed");
    CHECK(std::string(toString(ObjType::EndWall)) == "end_wall");
    CHECK(std::string(toString(ObjType::Deco)) == "deco");
}

TEST_CASE("duration without portals is length / start speed") {
    Level lv;
    // The run covers left-edge x from 0 to endX - kPlayerSize.
    const double run = 400.0 - phys::kPlayerSize;
    lv.endX = 400;
    lv.startSpeed = Speed::Normal;
    CHECK(levelDurationSeconds(lv) == doctest::Approx(run / phys::kSpeedNormal));
    lv.startSpeed = Speed::Slow;
    CHECK(levelDurationSeconds(lv) == doctest::Approx(run / phys::kSpeedSlow));
}

TEST_CASE("duration follows a speed portal") {
    Level lv;
    lv.endX = 300;
    lv.startSpeed = Speed::Normal;
    lv.objects.push_back(speedPortal(100, Speed::Fast));
    const double expect = 100.0 / phys::kSpeedNormal + (200.0 - phys::kPlayerSize) / phys::kSpeedFast;
    CHECK(levelDurationSeconds(lv) == doctest::Approx(expect));
}

TEST_CASE("duration with several portals, given out of order, and other objects mixed in") {
    Level lv;
    lv.endX = 400;
    lv.startSpeed = Speed::Slow;
    Object spike;
    spike.type = ObjType::Spike;
    spike.x = 50;
    lv.objects.push_back(speedPortal(300, Speed::Normal));  // out of order on purpose
    lv.objects.push_back(spike);
    lv.objects.push_back(speedPortal(100, Speed::Faster));
    const double expect = 100.0 / phys::kSpeedSlow + 200.0 / phys::kSpeedFaster +
                          (100.0 - phys::kPlayerSize) / phys::kSpeedNormal;
    CHECK(levelDurationSeconds(lv) == doctest::Approx(expect));
}

TEST_CASE("portals at or past the end wall are ignored; empty level is zero") {
    Level lv;
    lv.endX = 200;
    lv.startSpeed = Speed::Normal;
    lv.objects.push_back(speedPortal(199, Speed::Faster));  // at runEnd: ignored
    lv.objects.push_back(speedPortal(250, Speed::Slow));
    CHECK(levelDurationSeconds(lv) == doctest::Approx((200.0 - phys::kPlayerSize) / phys::kSpeedNormal));

    Level empty;
    CHECK(levelDurationSeconds(empty) == 0.0);
}

TEST_CASE("portal at x = 0 replaces the start speed") {
    Level lv;
    lv.endX = 100;
    lv.startSpeed = Speed::Slow;
    lv.objects.push_back(speedPortal(0, Speed::Fast));
    CHECK(levelDurationSeconds(lv) == doctest::Approx((100.0 - phys::kPlayerSize) / phys::kSpeedFast));
}

TEST_CASE("portal fires at left edge = portal.x, win at left edge = endX - size") {
    Level lv;
    lv.endX = 101;  // run end = 100
    lv.startSpeed = Speed::Slow;
    lv.objects.push_back(speedPortal(50, Speed::Fast));
    const double expect = 50.0 / phys::kSpeedSlow + 50.0 / phys::kSpeedFast;
    CHECK(levelDurationSeconds(lv) == doctest::Approx(expect));
}
