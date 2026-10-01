#include <doctest/doctest.h>

#include <string>

#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"
#include "core/Validator.h"

using namespace gd;

namespace {

Object mk(ObjType t, double x, double y, double w = 1, double h = 1) {
    Object o;
    o.type = t;
    o.x = x;
    o.y = y;
    o.w = w;
    o.h = h;
    return o;
}
Object block(double x, double y, double w = 1, double h = 1) { return mk(ObjType::Block, x, y, w, h); }
Object platform(double x, double y, double w = 1) { return mk(ObjType::Platform, x, y, w, phys::kPlatformHeight); }
Object spike(double x, double y = 0, SpikeDir d = SpikeDir::Up) {
    Object o = mk(ObjType::Spike, x, y);
    o.spikeDir = d;
    return o;
}
Object portalMode(double x, GameMode m) {
    Object o = mk(ObjType::PortalMode, x, 0, 1, phys::kPortalDrawHeight);
    o.mode = m;
    return o;
}
Object portalGrav(double x, Gravity g) {
    Object o = mk(ObjType::PortalGravity, x, 0, 1, phys::kPortalDrawHeight);
    o.gravity = g;
    return o;
}
Object portalSpeed(double x, Speed s) {
    Object o = mk(ObjType::PortalSpeed, x, 0, 1, phys::kPortalDrawHeight);
    o.speed = s;
    return o;
}

// A valid, empty level: end wall at 400 -> about 38 s at normal speed.
Level base() {
    Level lv;
    lv.id = 1;
    lv.name = "Test";
    lv.difficulty = 1;
    lv.music = "music/track01.ogg";
    lv.endX = 400;
    lv.objects.push_back(mk(ObjType::EndWall, 400, 0, 1, lv.ceiling));
    return lv;
}
void add(Level& lv, Object o) { lv.objects.push_back(o); }

int count(const ValidationReport& r, const std::string& rule) {
    int n = 0;
    for (const auto& i : r.issues)
        if (i.rule == rule) ++n;
    return n;
}
bool onlyRule(const ValidationReport& r, const std::string& rule) {
    return !r.issues.empty() && count(r, rule) == static_cast<int>(r.issues.size());
}

}  // namespace

TEST_CASE("validator: empty valid level passes") {
    Level lv = base();
    auto r = validateLevel(lv);
    CHECK(r.ok());
    CHECK(r.durationSeconds == doctest::Approx(399.0 / phys::kSpeedNormal));
}

TEST_CASE("validator: palette") {
    Level lv = base();
    CHECK(validateLevel(lv).ok());

    Level a = base();
    add(a, mk(ObjType::EndWall, 300, 0));
    CHECK(count(validateLevel(a), "palette") >= 1);

    Level b = base();
    add(b, block(20.3, 0));
    CHECK(onlyRule(validateLevel(b), "palette"));

    Level c = base();
    add(c, block(20, 11, 1, 2));  // above the ceiling
    CHECK(onlyRule(validateLevel(c), "palette"));

    Level d = base();
    d.id = 11;
    CHECK(onlyRule(validateLevel(d), "palette"));

    Level e = base();
    add(e, block(450, 0));  // right of the end wall
    CHECK(onlyRule(validateLevel(e), "palette"));

    Level f = base();
    f.ceiling = 30;
    CHECK(onlyRule(validateLevel(f), "palette"));

    Level g = base();
    g.name = "";
    CHECK(onlyRule(validateLevel(g), "palette"));

    Level h = base();
    add(h, mk(ObjType::Platform, 20, 0, 1, 1.0));  // wrong platform height
    CHECK(onlyRule(validateLevel(h), "palette"));
}

TEST_CASE("validator: embedded hazards") {
    Level ok = base();
    add(ok, block(20, 0));
    add(ok, spike(21, 0));          // beside the block
    add(ok, spike(20, 1));          // sitting on top of the block
    add(ok, spike(30, 11, SpikeDir::Down));  // hanging from the ceiling
    CHECK(count(validateLevel(ok), "embedded-hazard") == 0);

    Level bad = base();
    add(bad, block(20, 0, 3, 3));
    add(bad, spike(21, 1));
    auto r = validateLevel(bad);
    CHECK(count(r, "embedded-hazard") == 1);
}

TEST_CASE("validator: climb limit, gravity down") {
    Level ok = base();
    add(ok, block(30, 0, 1, 1));  // stair of 1-block steps
    add(ok, block(31, 0, 1, 2));
    add(ok, block(32, 0, 1, 3));
    add(ok, block(33, 0, 1, 4));
    add(ok, block(60, 0, 1, 2));  // 2-block wall
    add(ok, block(80, 0, 2, 1));
    CHECK(count(validateLevel(ok), "climb") == 0);

    Level wall3 = base();
    add(wall3, block(30, 0, 1, 3));
    auto r = validateLevel(wall3);
    CHECK(onlyRule(r, "climb"));
    CHECK(r.issues[0].x == 30);

    Level jump = base();  // a step of 2.5 after a step of 1
    add(jump, block(30, 0, 2, 1));
    add(jump, block(32, 0, 1, 4));
    CHECK(count(validateLevel(jump), "climb") == 1);

    Level stack = base();  // 3 stacked 1x1 blocks form a wall of 3
    add(stack, block(30, 0));
    add(stack, block(30, 1));
    add(stack, block(30, 2));
    CHECK(count(validateLevel(stack), "climb") >= 1);

    Level stack2 = base();  // 2 stacked blocks: fine
    add(stack2, block(30, 0));
    add(stack2, block(30, 1));
    CHECK(count(validateLevel(stack2), "climb") == 0);

    Level above = base();  // a high platform the cube passes under is not a wall
    add(above, platform(30, 3, 6));
    add(above, platform(40, 1.5, 4));
    CHECK(count(validateLevel(above), "climb") == 0);

    Level tower = base();  // tall step down: no climb
    add(tower, block(30, 0, 2, 4));  // this itself is a 4-high wall
    add(tower, block(32, 0, 2, 3));
    auto rt = validateLevel(tower);
    CHECK(count(rt, "climb") == 1);  // only the first
    CHECK(rt.issues[0].x == 30);
}

TEST_CASE("validator: climb limit, gravity up mirrors") {
    // ceiling 12: blocks hang from the ceiling
    Level ok = base();
    add(ok, portalGrav(10, Gravity::Up));
    add(ok, block(30, 11, 1, 1));
    add(ok, block(31, 10, 1, 2));
    add(ok, block(60, 10, 1, 2));
    CHECK(count(validateLevel(ok), "climb") == 0);

    Level bad = base();
    add(bad, portalGrav(10, Gravity::Up));
    add(bad, block(30, 9, 1, 3));  // 3-high wall hanging from the ceiling
    CHECK(count(validateLevel(bad), "climb") == 1);

    Level notMirrored = base();  // a ground wall is far from the ceiling "floor": cube passes beneath
    add(notMirrored, portalGrav(10, Gravity::Up));
    add(notMirrored, block(30, 0, 1, 3));
    CHECK(count(validateLevel(notMirrored), "climb") == 0);
}

TEST_CASE("validator: spike-run limit") {
    const int maxNormal = static_cast<int>(phys::cubeMaxSpikeRun(phys::kSpeedNormal));
    REQUIRE(maxNormal >= 3);

    Level ok = base();
    for (int i = 0; i < maxNormal; ++i) add(ok, spike(30 + i));
    CHECK(count(validateLevel(ok), "spike-run") == 0);

    Level bad = base();
    for (int i = 0; i <= maxNormal; ++i) add(bad, spike(30 + i));
    auto r = validateLevel(bad);
    CHECK(onlyRule(r, "spike-run"));
    CHECK(r.issues[0].x == 30);

    // a gap of one empty cell between spikes lets the cube land: two runs
    Level split = base();
    for (int i = 0; i < maxNormal; ++i) add(split, spike(30 + i));
    for (int i = 0; i < maxNormal; ++i) add(split, spike(30 + maxNormal + 1 + i));
    CHECK(count(validateLevel(split), "spike-run") == 0);

    // speed matters
    Level slow = base();
    slow.startSpeed = Speed::Slow;
    const int maxSlow = static_cast<int>(phys::cubeMaxSpikeRun(phys::kSpeedSlow));
    for (int i = 0; i <= maxSlow; ++i) add(slow, spike(30 + i));
    CHECK(count(validateLevel(slow), "spike-run") == 1);

    Level fast = base();
    add(fast, portalSpeed(10, Speed::Faster));
    for (int i = 0; i <= maxNormal; ++i) add(fast, spike(30 + i));
    CHECK(count(validateLevel(fast), "spike-run") == 0);

    // gravity up: ceiling spikes (dir=down) are the floor
    Level up = base();
    add(up, portalGrav(10, Gravity::Up));
    for (int i = 0; i <= maxNormal; ++i) add(up, spike(30 + i, 11, SpikeDir::Down));
    CHECK(count(validateLevel(up), "spike-run") == 1);

    // spikes at different heights are separate runs
    Level steps = base();
    add(steps, block(29, 0, 1, 3));
    add(steps, spike(30, 3));
    add(steps, block(31, 0, 1, 1));
    CHECK(count(validateLevel(steps), "spike-run") == 0);
}

TEST_CASE("validator: ship corridor gap") {
    Level ok = base();
    add(ok, portalMode(20, GameMode::Ship));
    add(ok, block(40, 0, 10, 4));       // floor 4, ceiling 12: gap 8
    add(ok, block(60, 0, 10, 4));
    add(ok, block(60, 6.5, 10, 5.5));   // gap exactly 2.5
    add(ok, portalMode(100, GameMode::Cube));
    CHECK(validateLevel(ok).ok());

    Level bad = base();
    add(bad, portalMode(20, GameMode::Ship));
    add(bad, block(60, 0, 10, 4));
    add(bad, block(60, 6, 10, 6));      // gap 2
    add(bad, portalMode(100, GameMode::Cube));
    auto r = validateLevel(bad);
    CHECK(onlyRule(r, "ship-gap"));
    CHECK(r.issues.size() == 1);
    CHECK(r.issues[0].x == doctest::Approx(60).epsilon(0.01));

    // a spike narrows the corridor
    Level sp = base();
    add(sp, portalMode(20, GameMode::Ship));
    add(sp, spike(64, 0));
    add(sp, block(60, 3, 10, 9));   // gap 3 above the floor, 2.45 above the spike hitbox
    add(sp, portalMode(100, GameMode::Cube));
    CHECK(count(validateLevel(sp), "ship-gap") >= 1);

    // the same blocks in a cube section are not a ship corridor
    Level cube = base();
    add(cube, block(60, 6, 10, 6));
    CHECK(count(validateLevel(cube), "ship-gap") == 0);

    // a level that starts in ship mode
    Level start = base();
    start.startMode = GameMode::Ship;
    add(start, block(60, 0, 10, 5));
    add(start, block(60, 7, 10, 5));
    CHECK(count(validateLevel(start), "ship-gap") >= 1);
}

TEST_CASE("validator: ship corridor slope") {
    // gap 2.5 corridors: [2, 4.5] then [6.5, 9]: the centre jumps by 4 blocks
    auto corridor = [](double floorA, double floorB, double len) {
        Level lv = base();
        add(lv, portalMode(20, GameMode::Ship));
        add(lv, block(40, 0, 10, floorA));
        add(lv, block(40, floorA + 2.5, 10, 12 - floorA - 2.5));
        add(lv, block(50, 0, len, floorB));
        add(lv, block(50, floorB + 2.5, len, 12 - floorB - 2.5));
        add(lv, portalMode(100, GameMode::Cube));
        return lv;
    };
    CHECK(onlyRule(validateLevel(corridor(2, 6.5, 10)), "ship-slope"));
    CHECK(validateLevel(corridor(2, 2, 10)).ok());
    // a shift of 1 block after a long enough run is reachable (0.5 per block)
    Level gentle = base();
    add(gentle, portalMode(20, GameMode::Ship));
    for (int k = 0; k < 4; ++k) {
        const double fl = 2 + k;  // 2,3,4,5
        add(gentle, block(40 + 6 * k, 0, 6, fl));
        add(gentle, block(40 + 6 * k, fl + 2.5, 6, 12 - fl - 2.5));
    }
    add(gentle, portalMode(100, GameMode::Cube));
    CHECK(validateLevel(gentle).ok());
    // the same shifts every 1 block are too steep
    Level steep = base();
    add(steep, portalMode(20, GameMode::Ship));
    for (int k = 0; k < 5; ++k) {
        const double fl = 2 + 1.5 * k;
        add(steep, block(40 + k, 0, 1, fl));
        add(steep, block(40 + k, fl + 2.5, 1, 12 - fl - 2.5));
    }
    add(steep, portalMode(100, GameMode::Cube));
    CHECK(count(validateLevel(steep), "ship-slope") >= 1);
}

TEST_CASE("validator: duration") {
    Level shortL = base();
    shortL.endX = 200;
    shortL.objects.back().x = 200;
    auto r = validateLevel(shortL);
    CHECK(onlyRule(r, "duration"));
    CHECK(r.issues[0].x < 0);

    Level longL = base();
    longL.endX = 800;
    longL.objects.back().x = 800;
    CHECK(onlyRule(validateLevel(longL), "duration"));

    // speed portals count: slow at the start for the whole level
    Level slow = base();
    slow.endX = 280;
    slow.objects.back().x = 280;
    CHECK(count(validateLevel(slow), "duration") == 1);  // 279/10.4 = 26.9 s
    slow.startSpeed = Speed::Slow;                       // 279/8.4 = 33.3 s
    CHECK(count(validateLevel(slow), "duration") == 0);
}

TEST_CASE("validator: start clear") {
    Level ok = base();
    add(ok, spike(8, 0));
    add(ok, portalSpeed(2, Speed::Normal));  // portals are fine
    CHECK(validateLevel(ok).ok());

    Level bad = base();
    add(bad, spike(7.5, 0));
    add(bad, block(3, 0));
    auto r = validateLevel(bad);
    CHECK(count(r, "start-clear") == 2);
}

TEST_CASE("validator: music file") {
    Level lv = base();
    CHECK(validateLevel(lv).ok());                 // no data dir: skipped
    CHECK(validateLevel(lv, GD_DATA_DIR).ok());    // assets/music/track01.ogg exists
    lv.music = "music/does_not_exist.ogg";
    CHECK(validateLevel(lv).ok());
    auto r = validateLevel(lv, GD_DATA_DIR);
    CHECK(onlyRule(r, "music"));
}

TEST_CASE("validator: a GD-like level passes") {
    Level lv = base();
    lv.endX = 420;
    lv.objects.back().x = 420;
    for (double x : {14.0, 22.0, 23.0, 24.0}) add(lv, spike(x));
    add(lv, block(34, 0, 1, 1));
    add(lv, block(35, 0, 1, 2));
    add(lv, block(36, 0, 1, 3));
    add(lv, block(37, 0, 3, 1));
    add(lv, block(55, 0, 1, 2));
    add(lv, platform(64, 1.5, 7));
    for (double x : {66.0, 67.0, 68.0}) add(lv, spike(x));
    add(lv, portalSpeed(100, Speed::Fast));
    add(lv, portalMode(140, GameMode::Ship));
    add(lv, block(150, 0, 20, 2));
    add(lv, block(150, 10, 20, 2));
    add(lv, portalMode(224, GameMode::Cube));
    add(lv, portalGrav(240, Gravity::Up));
    for (double x : {250.0, 251.0, 252.0}) add(lv, spike(x, 11, SpikeDir::Down));
    add(lv, block(258, 10, 1, 2));
    add(lv, portalGrav(275, Gravity::Down));
    add(lv, spike(300));
    auto r = validateLevel(lv, GD_DATA_DIR);
    for (const auto& i : r.issues) MESSAGE(i.rule << " x=" << i.x << " " << i.message);
    CHECK(r.ok());
}

TEST_CASE("validator: levels/_sample.json loads and passes") {
    std::vector<std::string> errs;
    auto lv = loadLevelFromFile(std::string(GD_DATA_DIR) + "/levels/_sample.json", errs);
    for (const auto& e : errs) MESSAGE(e);
    REQUIRE(lv.has_value());
    auto r = validateLevel(*lv, GD_DATA_DIR);
    for (const auto& i : r.issues) MESSAGE(i.rule << " x=" << i.x << " " << i.message);
    CHECK(r.ok());
    CHECK(r.durationSeconds >= phys::kLevelMinSeconds);
}
