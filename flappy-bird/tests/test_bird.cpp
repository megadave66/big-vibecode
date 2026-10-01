#include <doctest/doctest.h>

#include <cmath>

#include "core/bird.hpp"
#include "core/config.hpp"

using namespace flappy;
using namespace flappy::config;
using doctest::Approx;

namespace {

Bird fresh() {
    Bird b;
    bird_reset(b);
    return b;
}

float simulate_after_flap(float dt, float seconds) {
    Bird b = fresh();
    bird_flap(b);
    const int steps = static_cast<int>(seconds / dt + 0.5f);
    for (int i = 0; i < steps; ++i) {
        bird_update(b, dt);
    }
    return b.y;
}

Bird with_hitbox_bottom(float bottom) {
    Bird b = fresh();
    b.y = bottom - (kBirdH / 2.0f - kBirdHitboxInset);
    return b;
}

}  // namespace

TEST_CASE("bird: reset puts bird at start with zero velocity and angle") {
    Bird b;
    b.x = 1.0f;
    b.y = 2.0f;
    b.vy = 3.0f;
    b.angle = 4.0f;
    bird_reset(b);
    CHECK(b.x == Approx(kBirdX));
    CHECK(b.y == Approx(kBirdStartY));
    CHECK(b.vy == Approx(0.0f));
    CHECK(b.angle == Approx(0.0f));
}

TEST_CASE("bird: flap sets velocity and does not stack") {
    Bird b = fresh();
    bird_flap(b);
    CHECK(b.vy == Approx(kFlapVelocity));
    bird_flap(b);
    CHECK(b.vy == Approx(kFlapVelocity));
    b.vy = 300.0f;
    bird_flap(b);
    CHECK(b.vy == Approx(kFlapVelocity));
}

TEST_CASE("bird: gravity adds kGravity*dt to vy each step") {
    Bird b = fresh();
    bird_update(b, kDt);
    CHECK(b.vy == Approx(kGravity * kDt));
    bird_update(b, kDt);
    CHECK(b.vy == Approx(2.0f * kGravity * kDt));
}

TEST_CASE("bird: vy never exceeds kMaxFallSpeed") {
    Bird b = fresh();
    for (int i = 0; i < 10 * kTickHz; ++i) {
        bird_update(b, kDt);
        REQUIRE(b.vy <= kMaxFallSpeed);
    }
    CHECK(b.vy == Approx(kMaxFallSpeed));
}

TEST_CASE("bird: after a flap it rises to the expected apex then falls") {
    Bird b = fresh();
    bird_flap(b);
    float min_y = b.y;
    bool fell = false;
    for (int i = 0; i < 2 * kTickHz; ++i) {
        bird_update(b, kDt);
        if (b.y < min_y) {
            min_y = b.y;
        } else if (b.y > min_y + 1.0f) {
            fell = true;
        }
    }
    const float expected = kFlapVelocity * kFlapVelocity / (2.0f * kGravity);
    CHECK(std::abs((kBirdStartY - min_y) - expected) < 3.0f);
    CHECK(min_y < kBirdStartY);
    CHECK(fell);
}

TEST_CASE("bird: height after 1 s does not depend on the step size") {
    const float y120 = simulate_after_flap(1.0f / 120.0f, 1.0f);
    const float y240 = simulate_after_flap(1.0f / 240.0f, 1.0f);
    const float y60 = simulate_after_flap(1.0f / 60.0f, 1.0f);
    CHECK(std::abs(y120 - y240) < 3.0f);
    CHECK(std::abs(y120 - y60) < 3.0f);
    CHECK(std::abs(y240 - y60) < 3.0f);
}

TEST_CASE("bird: angle stays inside its limits") {
    Bird b = fresh();
    for (int i = 0; i < 5 * kTickHz; ++i) {
        if (i % 40 == 0) {
            bird_flap(b);
        }
        bird_update(b, kDt);
        REQUIRE(b.angle >= kMaxUpAngle);
        REQUIRE(b.angle <= kMaxDownAngle);
    }
    for (int i = 0; i < 5 * kTickHz; ++i) {
        bird_update(b, kDt);
        REQUIRE(b.angle >= kMaxUpAngle);
        REQUIRE(b.angle <= kMaxDownAngle);
    }
}

TEST_CASE("bird: nose points up right after a flap") {
    Bird b = fresh();
    bird_flap(b);
    CHECK(b.angle == Approx(kMaxUpAngle));
    bird_update(b, kDt);
    CHECK(b.angle == Approx(kMaxUpAngle));
}

TEST_CASE("bird: a long fall reaches the max down angle") {
    Bird b = fresh();
    bird_flap(b);
    for (int i = 0; i < 5 * kTickHz; ++i) {
        bird_update(b, kDt);
    }
    CHECK(b.angle == Approx(kMaxDownAngle));
}

TEST_CASE("bird: hitbox is the inset sprite box centred on the bird") {
    Bird b = fresh();
    b.x = 100.0f;
    b.y = 200.0f;
    const Rect r = bird_hitbox(b);
    CHECK(r.w == Approx(kBirdW - 2.0f * kBirdHitboxInset));
    CHECK(r.h == Approx(kBirdH - 2.0f * kBirdHitboxInset));
    CHECK(r.x + r.w / 2.0f == Approx(b.x));
    CHECK(r.y + r.h / 2.0f == Approx(b.y));
}

TEST_CASE("bird: hit_ground at the exact boundary") {
    CHECK(bird_hit_ground(with_hitbox_bottom(kGroundY)));
    CHECK(bird_hit_ground(with_hitbox_bottom(kGroundY + 5.0f)));
    CHECK_FALSE(bird_hit_ground(with_hitbox_bottom(kGroundY - 0.5f)));
}

TEST_CASE("bird: hit_ceiling at the exact boundary") {
    Bird b = fresh();
    const float half = kBirdH / 2.0f - kBirdHitboxInset;
    b.y = half;  // hitbox top == 0
    CHECK(bird_hit_ceiling(b));
    b.y = half - 5.0f;
    CHECK(bird_hit_ceiling(b));
    b.y = half + 0.5f;
    CHECK_FALSE(bird_hit_ceiling(b));
}

TEST_CASE("bird: land leaves the bird on the ground at rest") {
    Bird b = fresh();
    b.y = kGroundY + 30.0f;
    b.vy = 500.0f;
    bird_land(b);
    CHECK(bird_hit_ground(b));
    CHECK(b.vy == Approx(0.0f));
    CHECK(bird_hitbox(b).y + bird_hitbox(b).h == Approx(kGroundY));
    CHECK(b.angle == Approx(kMaxDownAngle));
}
