#include "Options.hpp"

#include <doctest/doctest.h>

#include <stdexcept>

TEST_SUITE("Options") {
  TEST_CASE("parse --frames") {
    const char* argv[] = {"baccarat", "--frames", "120"};
    auto opts = bac::app::parseOptions(3, const_cast<char**>(argv));
    REQUIRE(opts.frames.has_value());
    CHECK(opts.frames.value() == 120);
  }

  TEST_CASE("parse --screenshot") {
    const char* argv[] = {"baccarat", "--screenshot", "/tmp/test.bmp"};
    auto opts = bac::app::parseOptions(3, const_cast<char**>(argv));
    REQUIRE(opts.screenshot.has_value());
    CHECK(opts.screenshot.value() == "/tmp/test.bmp");
  }

  TEST_CASE("parse --seed") {
    const char* argv[] = {"baccarat", "--seed", "12345"};
    auto opts = bac::app::parseOptions(3, const_cast<char**>(argv));
    REQUIRE(opts.seed.has_value());
    CHECK(opts.seed.value() == 12345);
  }

  TEST_CASE("parse --script") {
    const char* argv[] = {"baccarat", "--script", "chip:25,bet:player"};
    auto opts = bac::app::parseOptions(3, const_cast<char**>(argv));
    REQUIRE(opts.script.has_value());
    CHECK(opts.script.value() == "chip:25,bet:player");
  }

  TEST_CASE("parse multiple options") {
    const char* argv[] = {"baccarat", "--frames", "60", "--seed", "999", "--screenshot", "/tmp/out.bmp"};
    auto opts = bac::app::parseOptions(7, const_cast<char**>(argv));
    CHECK(opts.frames.value() == 60);
    CHECK(opts.seed.value() == 999);
    CHECK(opts.screenshot.value() == "/tmp/out.bmp");
  }

  TEST_CASE("parse no options") {
    const char* argv[] = {"baccarat"};
    auto opts = bac::app::parseOptions(1, const_cast<char**>(argv));
    CHECK(!opts.frames.has_value());
    CHECK(!opts.screenshot.has_value());
    CHECK(!opts.seed.has_value());
    CHECK(!opts.script.has_value());
  }

  TEST_CASE("unknown option throws") {
    const char* argv[] = {"baccarat", "--unknown"};
    CHECK_THROWS_AS(bac::app::parseOptions(2, const_cast<char**>(argv)), std::invalid_argument);
  }

  TEST_CASE("missing argument throws") {
    const char* argv[] = {"baccarat", "--frames"};
    CHECK_THROWS_AS(bac::app::parseOptions(2, const_cast<char**>(argv)), std::invalid_argument);
  }

  TEST_CASE("invalid number throws") {
    const char* argv[] = {"baccarat", "--frames", "notanumber"};
    CHECK_THROWS_AS(bac::app::parseOptions(3, const_cast<char**>(argv)), std::invalid_argument);
  }
}
