#include <doctest/doctest.h>

#include "core/config.hpp"

using namespace flappy::config;

TEST_CASE("config: gap range is ordered") {
    CHECK(kGapMin <= kGapMax);
}

TEST_CASE("config: biggest gap fits between the margins") {
    CHECK(kGapMarginTop + kGapMax + kGapMarginBottom <= kGroundY);
}
