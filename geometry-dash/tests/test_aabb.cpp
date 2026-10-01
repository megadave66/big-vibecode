#include <doctest/doctest.h>

#include "core/Aabb.h"

using gd::Aabb;

TEST_CASE("overlap is strict: touching edges do not overlap") {
    const Aabb a{0, 0, 1, 1};
    CHECK(gd::overlaps(a, Aabb{0.5, 0.5, 1, 1}));
    CHECK_FALSE(gd::overlaps(a, Aabb{1, 0, 1, 1}));      // right edge touch
    CHECK_FALSE(gd::overlaps(a, Aabb{-1, 0, 1, 1}));     // left edge touch
    CHECK_FALSE(gd::overlaps(a, Aabb{0, 1, 1, 1}));      // top edge touch
    CHECK_FALSE(gd::overlaps(a, Aabb{0, -1, 1, 1}));     // bottom edge touch
    CHECK_FALSE(gd::overlaps(a, Aabb{1, 1, 1, 1}));      // corner touch
    CHECK(gd::overlaps(a, Aabb{0.999, 0.999, 1, 1}));
}

TEST_CASE("overlap is symmetric and handles containment and identity") {
    const Aabb big{0, 0, 10, 10}, small{4, 4, 1, 1};
    CHECK(gd::overlaps(big, small));
    CHECK(gd::overlaps(small, big));
    CHECK(gd::overlaps(big, big));
}

TEST_CASE("zero-size box on the edge of another box does not overlap") {
    CHECK_FALSE(gd::overlaps(Aabb{2, 1, 0, 0}, Aabb{0, 0, 2, 2}));
    CHECK_FALSE(gd::overlaps(Aabb{5, 5, 0, 0}, Aabb{0, 0, 2, 2}));
}

TEST_CASE("intersection") {
    const Aabb r = gd::intersection(Aabb{0, 0, 2, 2}, Aabb{1, 1, 2, 2});
    CHECK(r.x == doctest::Approx(1));
    CHECK(r.y == doctest::Approx(1));
    CHECK(r.w == doctest::Approx(1));
    CHECK(r.h == doctest::Approx(1));

    const Aabb none = gd::intersection(Aabb{0, 0, 1, 1}, Aabb{5, 5, 1, 1});
    CHECK(none.w == 0);
    CHECK(none.h == 0);
    const Aabb touch = gd::intersection(Aabb{0, 0, 1, 1}, Aabb{1, 0, 1, 1});
    CHECK(touch.w == 0);

    const Aabb inner = gd::intersection(Aabb{0, 0, 10, 10}, Aabb{2, 3, 1, 1});
    CHECK(inner.x == 2);
    CHECK(inner.y == 3);
    CHECK(inner.w == 1);
    CHECK(inner.h == 1);
}

TEST_CASE("contains point: low edges inclusive, high edges exclusive") {
    const Aabb b{0, 0, 2, 2};
    CHECK(gd::contains(b, 1, 1));
    CHECK(gd::contains(b, 0, 0));
    CHECK_FALSE(gd::contains(b, 2, 1));
    CHECK_FALSE(gd::contains(b, 1, 2));
    CHECK_FALSE(gd::contains(b, -0.001, 1));
}

TEST_CASE("contains box") {
    const Aabb outer{0, 0, 10, 10};
    CHECK(gd::contains(outer, Aabb{1, 1, 2, 2}));
    CHECK(gd::contains(outer, outer));
    CHECK_FALSE(gd::contains(outer, Aabb{9, 9, 2, 2}));
    CHECK_FALSE(gd::contains(Aabb{1, 1, 2, 2}, outer));
}

TEST_CASE("translate keeps size") {
    const Aabb t = gd::translate(Aabb{1, 2, 3, 4}, -1.5, 0.5);
    CHECK(t.x == doctest::Approx(-0.5));
    CHECK(t.y == doctest::Approx(2.5));
    CHECK(t.w == 3);
    CHECK(t.h == 4);
}
