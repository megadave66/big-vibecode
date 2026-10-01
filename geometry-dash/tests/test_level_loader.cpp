#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"

using namespace gd;

namespace {

std::string level(const std::string& objects, const std::string& extra = "") {
    return R"({"format":1,"id":2,"name":"T","difficulty":3,"music":"music/track02.ogg")" + extra +
           R"(,"objects":[)" + objects + "]}";
}
const std::string kEnd = R"({"type":"end_wall","x":400})";

struct Result {
    std::optional<Level> lv;
    std::vector<std::string> errors;
};
Result load(const std::string& json) {
    Result r;
    r.lv = loadLevelFromString(json, r.errors);
    return r;
}
bool hasError(const Result& r, const std::string& needle) {
    for (const auto& e : r.errors)
        if (e.find(needle) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("loader: valid minimal level with defaults") {
    auto r = load(level(kEnd));
    REQUIRE(r.lv.has_value());
    CHECK(r.errors.empty());
    const Level& lv = *r.lv;
    CHECK(lv.format == 1);
    CHECK(lv.id == 2);
    CHECK(lv.name == "T");
    CHECK(lv.difficulty == 3);
    CHECK(lv.music == "music/track02.ogg");
    CHECK(lv.startMode == GameMode::Cube);
    CHECK(lv.startSpeed == Speed::Normal);
    CHECK(lv.startGravity == Gravity::Down);
    CHECK(lv.ceiling == phys::kDefaultCeiling);
    CHECK(lv.endX == 400);
    CHECK(lv.objects.size() == 1);
}

TEST_CASE("loader: start, ceiling and colors") {
    auto r = load(level(kEnd, R"(,"start":{"mode":"ship","speed":"faster","gravity":"up"},"ceiling":10.5,
        "colors":{"bg":"#010203","ground":"#0A0b0C","accent":"#ffffff"})"));
    REQUIRE(r.lv.has_value());
    CHECK(r.lv->startMode == GameMode::Ship);
    CHECK(r.lv->startSpeed == Speed::Faster);
    CHECK(r.lv->startGravity == Gravity::Up);
    CHECK(r.lv->ceiling == 10.5);
    CHECK(r.lv->bg.r == 1);
    CHECK(r.lv->bg.b == 3);
    CHECK(r.lv->ground.r == 10);
    CHECK(r.lv->ground.g == 11);
    CHECK(r.lv->accent.r == 255);
}

TEST_CASE("loader: every object type, with defaults") {
    auto r = load(level(R"(
        {"type":"block","x":10,"y":0},
        {"type":"block","x":12,"y":1,"w":2.5,"h":3},
        {"type":"platform","x":20,"y":2},
        {"type":"platform","x":22,"y":2,"w":4},
        {"type":"spike","x":30,"y":0},
        {"type":"spike","x":31,"y":11,"dir":"down"},
        {"type":"portal_gravity","x":40,"gravity":"up"},
        {"type":"portal_mode","x":41,"y":1,"mode":"ship"},
        {"type":"portal_speed","x":42,"speed":"fast"},
        {"type":"deco","x":50,"y":0,"kind":"star"},
        {"type":"deco","x":51,"y":0,"w":2,"h":4,"kind":"pillar"},
        {"type":"end_wall","x":100})"));
    REQUIRE(r.lv.has_value());
    const auto& o = r.lv->objects;
    REQUIRE(o.size() == 12);
    CHECK((o[0].type == ObjType::Block));
    CHECK(o[0].w == 1);
    CHECK(o[0].h == 1);
    CHECK(o[1].w == 2.5);
    CHECK(o[1].h == 3);
    CHECK((o[2].type == ObjType::Platform));
    CHECK(o[2].w == 1);
    CHECK(o[2].h == phys::kPlatformHeight);
    CHECK(o[3].w == 4);
    CHECK((o[4].type == ObjType::Spike));
    CHECK(o[4].spikeDir == SpikeDir::Up);
    CHECK(o[4].w == 1);
    CHECK(o[4].h == 1);
    CHECK(o[5].spikeDir == SpikeDir::Down);
    CHECK((o[6].type == ObjType::PortalGravity));
    CHECK(o[6].gravity == Gravity::Up);
    CHECK(o[6].y == 0);
    CHECK(o[6].h == phys::kPortalDrawHeight);
    CHECK(o[7].mode == GameMode::Ship);
    CHECK(o[7].y == 1);
    CHECK(o[8].speed == Speed::Fast);
    CHECK((o[9].type == ObjType::Deco));
    CHECK(o[9].deco == DecoKind::Star);
    CHECK(o[9].w == 1);
    CHECK(o[10].deco == DecoKind::Pillar);
    CHECK(o[10].h == 4);
    CHECK((o[11].type == ObjType::EndWall));
    CHECK(r.lv->endX == 100);
}

TEST_CASE("loader: objects are sorted by x, stable for ties") {
    auto r = load(level(R"(
        {"type":"spike","x":30,"y":0},
        {"type":"block","x":10,"y":0},
        {"type":"spike","x":10,"y":5},
        {"type":"end_wall","x":100})"));
    REQUIRE(r.lv.has_value());
    const auto& o = r.lv->objects;
    CHECK((o[0].type == ObjType::Block));
    CHECK((o[1].type == ObjType::Spike));
    CHECK(o[1].x == 10);
    CHECK(o[2].x == 30);
    CHECK((o[3].type == ObjType::EndWall));
}

TEST_CASE("loader: malformed JSON") {
    auto r = load("{ not json");
    CHECK_FALSE(r.lv.has_value());
    CHECK(hasError(r, "json"));
    auto r2 = load("");
    CHECK_FALSE(r2.lv.has_value());
    auto r3 = load("[1,2]");
    CHECK_FALSE(r3.lv.has_value());
    CHECK(hasError(r3, "must be an object"));
}

TEST_CASE("loader: unknown keys are errors with a JSON path") {
    auto r = load(level(R"({"type":"block","x":1,"y":0,"color":"red"},)" + kEnd, R"(,"bogus":1)"));
    CHECK_FALSE(r.lv.has_value());
    CHECK(hasError(r, "bogus: unknown key"));
    CHECK(hasError(r, "objects[0].color: unknown key"));
    auto r2 = load(level(kEnd, R"(,"start":{"flavour":"x"},"colors":{"sky":"#000000"})"));
    CHECK(hasError(r2, "start.flavour: unknown key"));
    CHECK(hasError(r2, "colors.sky: unknown key"));
    // a field that exists for another type is unknown here
    auto r3 = load(level(R"({"type":"platform","x":1,"y":0,"h":2},)" + kEnd));
    CHECK(hasError(r3, "objects[0].h: unknown key"));
}

TEST_CASE("loader: bad enums") {
    CHECK(hasError(load(level(kEnd, R"(,"start":{"mode":"plane"})")), "start.mode: must be one of"));
    CHECK(hasError(load(level(kEnd, R"(,"start":{"speed":"warp"})")), "start.speed"));
    CHECK(hasError(load(level(kEnd, R"(,"start":{"gravity":"left"})")), "start.gravity"));
    CHECK(hasError(load(level(R"({"type":"spike","x":1,"y":0,"dir":"left"},)" + kEnd)),
                   "objects[0].dir"));
    CHECK(hasError(load(level(R"({"type":"deco","x":1,"y":0,"kind":"cloud"},)" + kEnd)),
                   "objects[0].kind"));
    CHECK(hasError(load(level(R"({"type":"portal_speed","x":1,"speed":"warp"},)" + kEnd)),
                   "objects[0].speed"));
    CHECK(hasError(load(level(R"({"type":"banana","x":1},)" + kEnd)), "unknown object type"));
}

TEST_CASE("loader: wrong types") {
    CHECK(hasError(load(R"({"format":"1","id":2,"name":"T","difficulty":3,"music":"m.ogg","objects":[]})"),
                   "format: must be an integer"));
    CHECK(hasError(load(level(R"({"type":"block","x":"1","y":0},)" + kEnd)), "objects[0].x: must be a number"));
    CHECK(hasError(load(level(R"(5,)" + kEnd)), "objects[0]: must be an object"));
    CHECK(hasError(load(R"({"format":1,"id":2.5,"name":"T","difficulty":3,"music":"m.ogg","objects":[]})"),
                   "id: must be an integer"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":7,"difficulty":3,"music":"m.ogg","objects":[]})"),
                   "name: must be a string"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"T","difficulty":3,"music":"m.ogg","objects":{}})"),
                   "objects: must be an array"));
    CHECK(hasError(load(level(kEnd, R"(,"start":3)")), "start: must be an object"));
}

TEST_CASE("loader: non-0.5 multiples") {
    auto r = load(level(R"({"type":"block","x":10.3,"y":0.25,"w":1.1,"h":1},)" + kEnd));
    CHECK_FALSE(r.lv.has_value());
    CHECK(hasError(r, "objects[0].x: must be a multiple of 0.5"));
    CHECK(hasError(r, "objects[0].y: must be a multiple of 0.5"));
    CHECK(hasError(r, "objects[0].w: must be a multiple of 0.5"));
    CHECK(load(level(R"({"type":"block","x":10.5,"y":0.5,"w":1.5,"h":0.5},)" + kEnd)).lv.has_value());
}

TEST_CASE("loader: out of range values") {
    CHECK(hasError(load(level(R"({"type":"block","x":10,"y":0,"w":0},)" + kEnd)), "objects[0].w: must be > 0"));
    CHECK(hasError(load(level(R"({"type":"block","x":10,"y":0,"h":-1},)" + kEnd)), "objects[0].h: must be > 0"));
    CHECK(hasError(load(level(R"({"type":"block","x":-1,"y":0},)" + kEnd)), "objects[0].x: must be >= 0"));
    CHECK(hasError(load(level(R"({"type":"block","x":1,"y":-1},)" + kEnd)), "objects[0].y: must be >= 0"));
    CHECK(hasError(load(level(R"({"type":"block","x":1,"y":11,"h":2},)" + kEnd)), "above the ceiling"));
    CHECK(hasError(load(level(R"({"type":"portal_mode","x":1,"y":10,"mode":"ship"},)" + kEnd)),
                   "above the ceiling"));
    CHECK(hasError(load(level(kEnd, R"(,"ceiling":5)")), "ceiling: must be in 6..20"));
    CHECK(hasError(load(level(kEnd, R"(,"ceiling":21)")), "ceiling"));
    CHECK(hasError(load(level(R"({"type":"block","x":1,"y":9,"h":3},)" + kEnd, R"(,"ceiling":10)")), "ceiling 10"));
    CHECK(hasError(load(R"({"format":2,"id":2,"name":"T","difficulty":3,"music":"m.ogg","objects":[]})"), "format: must be in 1..1"));
    CHECK(hasError(load(R"({"format":1,"id":11,"name":"T","difficulty":3,"music":"m.ogg","objects":[]})"), "id: must be in 1..10"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"T","difficulty":0,"music":"m.ogg","objects":[]})"), "difficulty: must be in 1..10"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"","difficulty":3,"music":"m.ogg","objects":[]})"), "name: must be 1..32"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"123456789012345678901234567890123","difficulty":3,"music":"m.ogg","objects":[]})"), "name: must be 1..32"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"T","difficulty":3,"music":"../x.ogg","objects":[]})"), "music"));
    CHECK(hasError(load(level(kEnd, R"(,"colors":{"bg":"red"})")), "colors.bg: must be"));
    CHECK(hasError(load(level(kEnd, R"(,"colors":{"bg":"#12345g"})")), "colors.bg: must be"));
}

TEST_CASE("loader: missing required fields") {
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"T","difficulty":3,"objects":[]})"), "music: missing required field"));
    CHECK(hasError(load(R"({"format":1,"id":2,"name":"T","difficulty":3,"music":"m.ogg"})"), "objects: missing required field"));
    CHECK(hasError(load(level(R"({"type":"block","y":0},)" + kEnd)), "objects[0].x: missing required field"));
    CHECK(hasError(load(level(R"({"type":"portal_mode","x":1},)" + kEnd)), "objects[0].mode: missing required field"));
    CHECK(hasError(load(level(R"({"x":1},)" + kEnd)), "objects[0].type: missing required field"));
}

TEST_CASE("loader: end_wall rules") {
    auto none = load(level(R"({"type":"block","x":1,"y":0})"));
    CHECK_FALSE(none.lv.has_value());
    CHECK(hasError(none, "missing end_wall"));

    auto two = load(level(R"({"type":"end_wall","x":100},{"type":"end_wall","x":200})"));
    CHECK_FALSE(two.lv.has_value());
    CHECK(hasError(two, "more than one end_wall"));

    auto notRight = load(level(R"({"type":"end_wall","x":100},{"type":"spike","x":150,"y":0})"));
    CHECK_FALSE(notRight.lv.has_value());
    CHECK(hasError(notRight, "objects[1].x"));
    CHECK(hasError(notRight, "right-most"));

    // objects at the same x as the end wall are fine
    CHECK(load(level(R"({"type":"end_wall","x":100},{"type":"deco","x":100,"y":0,"kind":"glow"})")).lv.has_value());
}

TEST_CASE("loader: all errors are collected, not just the first") {
    auto r = load(level(R"(
        {"type":"block","x":1.3,"y":0},
        {"type":"spike","x":2,"y":0,"dir":"sideways"},
        {"type":"block","x":3,"y":0,"w":0})",
                        R"(,"ceiling":99,"bogus":true)"));
    CHECK_FALSE(r.lv.has_value());
    CHECK(r.errors.size() >= 6);
    CHECK(hasError(r, "bogus"));
    CHECK(hasError(r, "ceiling"));
    CHECK(hasError(r, "objects[0].x"));
    CHECK(hasError(r, "objects[1].dir"));
    CHECK(hasError(r, "objects[2].w: must be > 0"));
    CHECK(hasError(r, "missing end_wall"));
}

TEST_CASE("loader: file helpers and file errors") {
    CHECK(levelPath("/d", 3) == "/d/levels/level03.json");
    CHECK(levelPath("/d", 10) == "/d/levels/level10.json");
    CHECK(assetPath("/d", "music/a.ogg") == "/d/assets/music/a.ogg");
    std::vector<std::string> errs;
    CHECK_FALSE(loadLevelFromFile("/nonexistent/level01.json", errs).has_value());
    REQUIRE(errs.size() == 1);
    CHECK(errs[0].find("cannot open") != std::string::npos);
}
