#include <doctest/doctest.h>

#include "core/collision.hpp"

using flappy::intersects;
using flappy::Rect;

TEST_CASE("collision: overlapping boxes intersect") {
    CHECK(intersects(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}));
    CHECK(intersects(Rect{5, 5, 10, 10}, Rect{0, 0, 10, 10}));
}

TEST_CASE("collision: disjoint boxes do not intersect") {
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{20, 0, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{0, 20, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{-30, -30, 10, 10}));
}

TEST_CASE("collision: touching edges and corners do not intersect") {
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{10, 0, 10, 10}));
    CHECK_FALSE(intersects(Rect{10, 0, 10, 10}, Rect{0, 0, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{0, 10, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{10, 10, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{-10, -10, 10, 10}));
}

TEST_CASE("collision: containment intersects") {
    CHECK(intersects(Rect{0, 0, 100, 100}, Rect{40, 40, 5, 5}));
    CHECK(intersects(Rect{40, 40, 5, 5}, Rect{0, 0, 100, 100}));
}

TEST_CASE("collision: zero-size boxes never intersect") {
    CHECK_FALSE(intersects(Rect{5, 5, 0, 0}, Rect{0, 0, 10, 10}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{5, 5, 0, 0}));
    CHECK_FALSE(intersects(Rect{5, 0, 0, 10}, Rect{0, 0, 10, 10}));
    CHECK_FALSE(intersects(Rect{5, 5, 0, 0}, Rect{5, 5, 0, 0}));
}
