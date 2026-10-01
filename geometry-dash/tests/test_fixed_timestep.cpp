#include <doctest/doctest.h>

#include "core/FixedTimestep.h"
#include "core/PhysicsConstants.h"

using gd::FixedTimestep;

TEST_CASE("one second of time gives kTickRate ticks") {
    FixedTimestep ft;
    int total = 0;
    for (int i = 0; i < 60; ++i) total += ft.advance(1.0 / 60.0);
    // Floating-point accumulation may leave the last tick pending; allow one.
    CHECK(total >= gd::phys::kTickRate - 1);
    CHECK(total <= gd::phys::kTickRate);
}

TEST_CASE("small dt accumulates until a full step") {
    FixedTimestep ft(100.0);  // step = 0.01
    CHECK(ft.advance(0.004) == 0);
    CHECK(ft.alpha() == doctest::Approx(0.4));
    CHECK(ft.advance(0.004) == 0);
    CHECK(ft.advance(0.004) == 1);
    CHECK(ft.alpha() == doctest::Approx(0.2).epsilon(1e-6));
}

TEST_CASE("alpha stays in [0,1)") {
    FixedTimestep ft;
    for (int i = 0; i < 1000; ++i) {
        ft.advance(0.0031 + 0.0001 * (i % 7));
        CHECK(ft.alpha() >= 0.0);
        CHECK(ft.alpha() < 1.0);
    }
}

TEST_CASE("spiral of death is capped at 0.25 s") {
    FixedTimestep ft;
    const int ticks = ft.advance(5.0);
    CHECK(ticks <= static_cast<int>(0.25 * gd::phys::kTickRate) + 1);
    CHECK(ticks >= static_cast<int>(0.25 * gd::phys::kTickRate) - 1);
}

TEST_CASE("negative and zero dt are harmless") {
    FixedTimestep ft;
    CHECK(ft.advance(-1.0) == 0);
    CHECK(ft.advance(0.0) == 0);
    CHECK(ft.alpha() == 0.0);
}

TEST_CASE("reset clears the accumulator") {
    FixedTimestep ft(100.0);
    ft.advance(0.007);
    ft.reset();
    CHECK(ft.alpha() == 0.0);
}
