#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/config.hpp"
#include "core/pipes.hpp"

using namespace flappy;
using namespace flappy::config;

namespace {
float center_of(const Gap& g) { return g.gap_top + g.gap_h / 2.0f; }
}  // namespace

TEST_CASE("pipes: gap bounds hold for many seeds") {
    const float eps = 1e-3f;
    for (std::uint32_t seed = 1; seed <= 50; ++seed) {
        GapGenerator gen(seed);
        float prev = 0.0f;
        for (int n = 0; n < 2000; ++n) {
            const Gap g = gen.next();
            REQUIRE(g.gap_h >= kGapMin - eps);
            REQUIRE(g.gap_h <= kGapMax + eps);
            REQUIRE(g.gap_top >= kGapMarginTop - eps);
            REQUIRE(g.gap_top + g.gap_h <= kGroundY - kGapMarginBottom + eps);
            if (n >= 1) {
                REQUIRE(std::fabs(center_of(g) - prev) <= kMaxGapCenterDelta + eps);
            }
            prev = center_of(g);
        }
    }
}

TEST_CASE("pipes: first gap is near the middle") {
    const float mid = (kGapMarginTop + kGroundY - kGapMarginBottom) / 2.0f;
    for (std::uint32_t seed = 1; seed <= 50; ++seed) {
        GapGenerator gen(seed);
        const Gap g = gen.next();
        CHECK(std::fabs(center_of(g) - mid) <= kMaxGapCenterDelta / 2.0f + 1e-3f);
    }
}

TEST_CASE("pipes: generator is deterministic") {
    auto run = [](GapGenerator& g) {
        std::vector<Gap> v;
        for (int i = 0; i < 100; ++i) v.push_back(g.next());
        return v;
    };
    GapGenerator a(42), b(42), c(43);
    const auto va = run(a);
    const auto vb = run(b);
    const auto vc = run(c);
    bool differs = false;
    for (std::size_t i = 0; i < va.size(); ++i) {
        CHECK(va[i].gap_top == vb[i].gap_top);
        CHECK(va[i].gap_h == vb[i].gap_h);
        if (va[i].gap_top != vc[i].gap_top) differs = true;
    }
    CHECK(differs);
    a.reset(42);
    const auto va2 = run(a);
    for (std::size_t i = 0; i < va.size(); ++i) {
        CHECK(va[i].gap_top == va2[i].gap_top);
        CHECK(va[i].gap_h == va2[i].gap_h);
    }
}

TEST_CASE("pipes: gap height really varies") {
    GapGenerator gen(7);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 500; ++i) {
        const Gap g = gen.next();
        lo = std::min(lo, g.gap_h);
        hi = std::max(hi, g.gap_h);
    }
    CHECK(hi - lo >= (kGapMax - kGapMin) / 2.0f);
}

TEST_CASE("pipes: rects match the gap") {
    Pipe p{100.0f, Gap{120.0f, 110.0f}, false};
    const Rect t = pipe_top_rect(p);
    const Rect b = pipe_bottom_rect(p);
    CHECK(t.x == doctest::Approx(100.0f));
    CHECK(t.y == doctest::Approx(0.0f));
    CHECK(t.h == doctest::Approx(120.0f));
    CHECK(t.w == doctest::Approx(kPipeWidth));
    CHECK(b.y == doctest::Approx(230.0f));
    CHECK(b.y + b.h == doctest::Approx(kGroundY));
    CHECK(b.w == doctest::Approx(kPipeWidth));

    PipeField f(1);
    // Build a field with one known pipe by scrolling until one spawns.
    while (f.pipes().empty()) f.update(kDt);
    const Pipe q = f.pipes().front();
    const float cy = q.gap.gap_top + q.gap.gap_h / 2.0f;
    CHECK_FALSE(f.collides(Rect{q.x + 5.0f, cy - 12.0f, 26.0f, 16.0f}));
    CHECK(f.collides(Rect{q.x + 5.0f, q.gap.gap_top - 5.0f, 26.0f, 16.0f}));
    CHECK(f.collides(Rect{q.x + 5.0f, q.gap.gap_top + q.gap.gap_h - 5.0f, 26.0f, 16.0f}));
}

TEST_CASE("pipes: spawning, spacing, removal, bounded count") {
    PipeField f(3);
    CHECK(f.pipes().empty());
    std::size_t max_count = 0;
    bool removed = false;
    for (int i = 0; i < 60 * kTickHz; ++i) {
        const std::size_t before = f.pipes().size();
        f.update(kDt);
        if (f.pipes().size() < before) removed = true;
        max_count = std::max(max_count, f.pipes().size());
        const auto& ps = f.pipes();
        for (std::size_t k = 1; k < ps.size(); ++k) {
            REQUIRE(ps[k].x - ps[k - 1].x == doctest::Approx(kPipeSpacing).epsilon(1e-3));
        }
        for (const Pipe& p : ps) REQUIRE(p.x + kPipeWidth >= 0.0f);
    }
    CHECK(removed);
    CHECK(max_count >= 2);
    CHECK(max_count <= static_cast<std::size_t>(kWorldWidth / kPipeSpacing) + 3);
    // After 60 s the first pipe must be long gone: leftmost pipe is recent.
    CHECK(f.pipes().front().x > -kPipeWidth);
}

TEST_CASE("pipes: scroll speed") {
    PipeField f(5);
    while (f.pipes().empty()) f.update(kDt);
    const float x0 = f.pipes().front().x;
    const int steps = kTickHz;  // 1 s
    for (int i = 0; i < steps; ++i) f.update(kDt);
    CHECK(x0 - f.pipes().front().x == doctest::Approx(kPipeSpeed * 1.0f).epsilon(1e-3));
}

TEST_CASE("pipes: score counts each pipe once") {
    PipeField f(9);
    int total = 0;
    int passed = 0;
    std::vector<float> rights;
    const float bird_x = kBirdX;
    for (int i = 0; i < 30 * kTickHz; ++i) {
        f.update(kDt);
        // Pipes whose right edge is already left of the bird and unscored will score now.
        int expected = 0;
        for (const Pipe& p : f.pipes()) {
            if (!p.scored && p.x + kPipeWidth < bird_x) ++expected;
        }
        const int got = f.collect_score(bird_x);
        CHECK(got == expected);
        CHECK(got <= 1);
        total += got;
        CHECK(f.collect_score(bird_x) == 0);
    }
    // Pipe n right edge reaches bird_x at: kFirstPipeX + n*spacing + width - bird_x over speed.
    const float elapsed = 30.0f;
    const float dist = kPipeSpeed * elapsed;
    for (int n = 0; ; ++n) {
        if (kFirstPipeX + static_cast<float>(n) * kPipeSpacing + kPipeWidth - bird_x < dist - 1.0f) ++passed;
        else break;
    }
    CHECK(total == passed);
    CHECK(total > 0);
}

TEST_CASE("pipes: reset clears pipes and replays the seed") {
    PipeField f(11);
    for (int i = 0; i < 20 * kTickHz; ++i) f.update(kDt);
    REQUIRE_FALSE(f.pipes().empty());
    f.reset(21);
    CHECK(f.pipes().empty());
    PipeField fresh(21);
    for (int i = 0; i < 20 * kTickHz; ++i) {
        f.update(kDt);
        fresh.update(kDt);
    }
    REQUIRE(f.pipes().size() == fresh.pipes().size());
    for (std::size_t i = 0; i < f.pipes().size(); ++i) {
        CHECK(f.pipes()[i].x == fresh.pipes()[i].x);
        CHECK(f.pipes()[i].gap.gap_top == fresh.pipes()[i].gap.gap_top);
        CHECK(f.pipes()[i].gap.gap_h == fresh.pipes()[i].gap.gap_h);
    }
}
