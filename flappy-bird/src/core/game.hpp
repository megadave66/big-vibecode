// Game state machine. Pure logic, no SDL. Owns the bird, pipes and score.
// The app layer feeds it input (press) and fixed ticks, then reads state to
// draw, and drains events to play sounds and save the best score.
#pragma once

#include <cstdint>

#include "bird.hpp"
#include "pipes.hpp"

namespace flappy {

enum class State {
    GetReady,  // bird bobs in place, no gravity, no pipes. press -> Playing (+flap)
    Playing,   // physics + pipes. hit pipe/ground/ceiling -> Dying
    Dying,     // pipes and ground stop; bird falls to the ground, no input. on ground -> GameOver
    GameOver,  // panel shown. press after kGameOverInputDelay -> GetReady (reset)
};

// One-shot events raised since the last take_events() call.
struct Events {
    bool flapped = false;
    bool scored = false;
    bool hit = false;       // raised once on the Playing -> Dying transition
    bool game_over = false; // raised once on entering GameOver
};

class Game {
public:
    Game(std::uint32_t seed, int best);

    // "Tap" input (space, click, up arrow). Meaning depends on state.
    void press();

    // Advance one fixed step of dt seconds.
    void tick(float dt);

    Events take_events();

    State state() const { return state_; }
    int score() const { return score_; }
    int best() const { return best_; }
    bool new_best() const { return new_best_; }  // true in GameOver if this run beat the old best
    const Bird& bird() const { return bird_; }
    const PipeField& pipes() const { return pipes_; }
    float ground_offset() const { return ground_offset_; }  // [0, kGroundPatternWidth) px for scrolling ground art
    float state_time() const { return state_time_; }         // seconds in current state

private:
    void enter(State s);

    State state_ = State::GetReady;
    Bird bird_;
    PipeField pipes_;
    std::uint32_t seed_;
    int score_ = 0;
    int best_ = 0;
    bool new_best_ = false;
    float ground_offset_ = 0.0f;
    float state_time_ = 0.0f;
    Events events_;
};

}  // namespace flappy
