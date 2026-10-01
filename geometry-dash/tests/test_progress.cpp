#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/Progress.h"

namespace {
std::string tmpPath(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "gd_progress_tests";
    std::filesystem::create_directories(dir);
    const auto p = dir / name;
    std::filesystem::remove(p);
    return p.string();
}
}  // namespace

TEST_CASE("progress: round trip keeps every field") {
    const std::string path = tmpPath("roundtrip.json");
    gd::Progress a(path);
    a.recordAttempt(3);
    a.recordAttempt(3);
    a.recordBest(3, false, 42);
    a.recordBest(3, true, 77);
    a.recordWin(5, false);
    CHECK(a.save());
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));

    gd::Progress b(path);
    CHECK(b.load());
    CHECK(b.get(3).attempts == 2);
    CHECK(b.get(3).bestPercentNormal == 42);
    CHECK(b.get(3).bestPercentPractice == 77);
    CHECK_FALSE(b.get(3).completed);
    CHECK(b.get(5).completed);
    CHECK(b.get(5).bestPercentNormal == 100);
    CHECK(b.get(5).bestPercentPractice == 0);
}

TEST_CASE("progress: missing file starts fresh") {
    gd::Progress p(tmpPath("missing.json"));
    CHECK_FALSE(p.load());
    CHECK(p.all().empty());
    CHECK(p.get(1).attempts == 0);
}

TEST_CASE("progress: corrupt files start fresh and never throw") {
    const char* bad[] = {"", "not json {{", "[1,2,3]", "{\"levels\": 5}", "{\"levels\": {\"x\": {\"attempts\": 1}}}",
                         "{\"levels\": {\"2\": 7}}"};
    for (const char* text : bad) {
        const std::string path = tmpPath("corrupt.json");
        { std::ofstream(path) << text; }
        gd::Progress p(path);
        p.recordAttempt(1);
        CHECK_NOTHROW(p.load());
        CHECK(p.all().empty());
        // The app can keep going and overwrite the bad file.
        p.recordAttempt(1);
        CHECK(p.save());
        gd::Progress q(path);
        CHECK(q.load());
        CHECK(q.get(1).attempts == 1);
    }
}

TEST_CASE("progress: out-of-range values are clamped on load") {
    const std::string path = tmpPath("clamp.json");
    { std::ofstream(path) << R"({"levels":{"1":{"attempts":-4,"best_normal":250,"best_practice":-9,"completed":true}}})"; }
    gd::Progress p(path);
    CHECK(p.load());
    CHECK(p.get(1).attempts == 0);
    CHECK(p.get(1).bestPercentNormal == 100);
    CHECK(p.get(1).bestPercentPractice == 0);
    CHECK(p.get(1).completed);
}

TEST_CASE("progress: best percent only increases, modes are separate") {
    gd::Progress p;
    p.recordBest(1, false, 30);
    p.recordBest(1, false, 20);
    CHECK(p.get(1).bestPercentNormal == 30);
    p.recordBest(1, false, 55);
    CHECK(p.get(1).bestPercentNormal == 55);
    p.recordBest(1, true, 10);
    CHECK(p.get(1).bestPercentPractice == 10);
    CHECK(p.get(1).bestPercentNormal == 55);
    p.recordBest(1, false, 400);
    CHECK(p.get(1).bestPercentNormal == 100);
}

TEST_CASE("progress: attempts increment; completion needs a normal win") {
    gd::Progress p;
    for (int i = 0; i < 5; ++i) p.recordAttempt(2);
    CHECK(p.get(2).attempts == 5);
    p.recordWin(2, true);
    CHECK_FALSE(p.get(2).completed);
    CHECK(p.get(2).bestPercentPractice == 100);
    p.recordWin(2, false);
    CHECK(p.get(2).completed);
    CHECK(p.get(2).attempts == 5);
}

TEST_CASE("progress: ids <= 0 are ignored; empty path does not persist") {
    gd::Progress p;
    p.recordAttempt(0);
    p.recordBest(-1, false, 50);
    CHECK(p.all().empty());
    p.recordAttempt(1);
    CHECK(p.save());
    CHECK(p.dirty());
}
