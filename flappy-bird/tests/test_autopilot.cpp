#include <doctest/doctest.h>

#include <cstdint>
#include <cstdio>

#include "core/autopilot.hpp"
#include "core/bird.hpp"
#include "core/config.hpp"
#include "core/pipes.hpp"

using namespace flappy;
using namespace flappy::config;

TEST_CASE("autopilot: gaps are passable") {
    int min_score = 1 << 30;
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        Bird bird;
        bird_reset(bird);
        PipeField pipes(seed);
        int score = 0;
        for (int i = 0; i < 60 * kTickHz; ++i) {
            if (autopilot_should_flap(bird, pipes)) bird_flap(bird);
            bird_update(bird, kDt);
            pipes.update(kDt);
            score += pipes.collect_score(bird.x);
            const bool hit = pipes.collides(bird_hitbox(bird));
            const bool ground = bird_hit_ground(bird);
            const bool ceiling = bird_hit_ceiling(bird);
            INFO("seed " << seed << " tick " << i);
            REQUIRE_FALSE(hit);
            REQUIRE_FALSE(ground);
            REQUIRE_FALSE(ceiling);
        }
        INFO("seed " << seed);
        CHECK(score >= 30);
        if (score < min_score) min_score = score;
    }
    std::printf("autopilot min score across seeds: %d\n", min_score);
}
