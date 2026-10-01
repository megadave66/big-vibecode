// Game state machine. See game.hpp for the contract.
#include "game.hpp"

#include <algorithm>
#include <cmath>

#include "config.hpp"

namespace flappy {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
// LCG step used to derive each new run's pipe seed from the previous one.
constexpr std::uint32_t kSeedMul = 1664525u;
constexpr std::uint32_t kSeedAdd = 1013904223u;

}  // namespace

Game::Game(std::uint32_t seed, int best) : pipes_(seed), seed_(seed), best_(std::max(0, best)) {
    bird_reset(bird_);
}

void Game::press() {
    switch (state_) {
        case State::GetReady:
            enter(State::Playing);
            bird_flap(bird_);
            events_.flapped = true;
            break;
        case State::Playing:
            bird_flap(bird_);
            events_.flapped = true;
            break;
        case State::Dying:
            break;
        case State::GameOver:
            if (state_time_ >= config::kGameOverInputDelay) {
                bird_reset(bird_);
                seed_ = seed_ * kSeedMul + kSeedAdd;
                pipes_.reset(seed_);
                score_ = 0;
                new_best_ = false;
                enter(State::GetReady);
            }
            break;
    }
}

void Game::tick(float dt) {
    state_time_ += dt;
    bool scroll = false;

    switch (state_) {
        case State::GetReady:
            bird_.y = config::kBirdStartY +
                      config::kBobAmplitude * std::sin(kTwoPi * config::kBobHz * state_time_);
            bird_.vy = 0.0f;
            bird_.angle = 0.0f;
            scroll = true;
            break;
        case State::Playing: {
            bird_update(bird_, dt);
            pipes_.update(dt);
            const int gained = pipes_.collect_score(bird_.x);
            if (gained > 0) {
                score_ += gained;
                events_.scored = true;
            }
            scroll = true;
            if (pipes_.collides(bird_hitbox(bird_)) || bird_hit_ground(bird_) ||
                bird_hit_ceiling(bird_)) {
                events_.hit = true;
                enter(State::Dying);  // a direct ground hit lands below on this same tick
            }
            break;
        }
        case State::Dying:
            bird_update(bird_, dt);
            break;
        case State::GameOver:
            break;
    }

    // Single exit from Dying: the bird has reached the ground.
    if (state_ == State::Dying && bird_hit_ground(bird_)) {
        bird_land(bird_);
        enter(State::GameOver);
        if (score_ > best_) {
            best_ = score_;
            new_best_ = true;
        }
        events_.game_over = true;
    }

    if (scroll) {
        ground_offset_ = std::fmod(ground_offset_ + config::kGroundScrollSpeed * dt, config::kGroundPatternWidth);
        if (ground_offset_ < 0.0f) {
            ground_offset_ += config::kGroundPatternWidth;
        }
    }
}

Events Game::take_events() {
    const Events e = events_;
    events_ = Events{};
    return e;
}

void Game::enter(State s) {
    state_ = s;
    state_time_ = 0.0f;
}

}  // namespace flappy
