#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/best_score.hpp"

namespace fs = std::filesystem;
using flappy::load_best;
using flappy::save_best;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("flappy_best_test_" + std::to_string(stamp));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_text(const fs::path& p, const std::string& s) {
    std::ofstream out(p, std::ios::binary);
    out << s;
}

}  // namespace

TEST_CASE("best: round trip save then load") {
    TempDir d;
    const fs::path f = d.path / "best.txt";
    CHECK(save_best(f, 17));
    CHECK(load_best(f) == 17);
    CHECK(save_best(f, 0));
    CHECK(load_best(f) == 0);
    CHECK(save_best(f, 123456));
    CHECK(load_best(f) == 123456);
}

TEST_CASE("best: missing file gives 0") {
    TempDir d;
    CHECK(load_best(d.path / "nope.txt") == 0);
}

TEST_CASE("best: garbage or negative or empty files give 0") {
    TempDir d;
    const fs::path f = d.path / "b.txt";
    write_text(f, "hello world\n");
    CHECK(load_best(f) == 0);
    write_text(f, "-5\n");
    CHECK(load_best(f) == 0);
    write_text(f, "");
    CHECK(load_best(f) == 0);
    write_text(f, "   \n");
    CHECK(load_best(f) == 0);
}

TEST_CASE("best: leading integer parsed and trailing junk ignored") {
    TempDir d;
    const fs::path f = d.path / "b.txt";
    write_text(f, "42  \n\n");
    CHECK(load_best(f) == 42);
    write_text(f, "9 extra");
    CHECK(load_best(f) == 9);
}

TEST_CASE("best: save creates missing parent dirs") {
    TempDir d;
    const fs::path f = d.path / "a" / "b" / "best.txt";
    CHECK(save_best(f, 3));
    CHECK(load_best(f) == 3);
}

TEST_CASE("best: save to an impossible path returns false without throwing") {
    TempDir d;
    const fs::path blocker = d.path / "file";
    write_text(blocker, "x");
    bool ok = true;
    CHECK_NOTHROW(ok = save_best(blocker / "sub" / "best.txt", 5));
    CHECK_FALSE(ok);
}

TEST_CASE("best: no .tmp file left after a good save") {
    TempDir d;
    const fs::path f = d.path / "best.txt";
    REQUIRE(save_best(f, 8));
    CHECK_FALSE(fs::exists(d.path / "best.txt.tmp"));
    int count = 0;
    for (const auto& e : fs::directory_iterator(d.path)) {
        (void)e;
        ++count;
    }
    CHECK(count == 1);
}
