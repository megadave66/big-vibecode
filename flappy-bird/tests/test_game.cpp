#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "core/autopilot.hpp"
#include "core/config.hpp"
#include "core/game.hpp"

using namespace flappy;
namespace cfg = flappy::config;

namespace {

// Run the autopilot for `seconds`; counts scored events. Stops early on game over.
int run_autopilot(Game& g, float seconds, int* scored_events = nullptr) {
    int events = 0;
    const int ticks = static_cast<int>(seconds / cfg::kDt);
    for (int i = 0; i < ticks; ++i) {
        if (g.state() == State::GetReady ||
            (g.state() == State::Playing && autopilot_should_flap(g.bird(), g.pipes()))) {
            g.press();
        }
        g.tick(cfg::kDt);
        if (g.take_events().scored) {
            ++events;
        }
        if (g.state() == State::GameOver) {
            break;
        }
    }
    if (scored_events != nullptr) {
        *scored_events = events;
    }
    return g.score();
}

// Let the bird fall to game over with no input. Returns hit/game_over counts.
void fall_to_game_over(Game& g, int& hits, int& overs) {
    hits = 0;
    overs = 0;
    for (int i = 0; i < 120 * 10 && g.state() != State::GameOver; ++i) {
        g.tick(cfg::kDt);
        const Events e = g.take_events();
        hits += e.hit ? 1 : 0;
        overs += e.game_over ? 1 : 0;
    }
}

void wait_in_state(Game& g, float seconds) {
    const int ticks = static_cast<int>(seconds / cfg::kDt) + 1;
    for (int i = 0; i < ticks; ++i) {
        g.tick(cfg::kDt);
    }
}

}  // namespace

TEST_CASE("game: starts in GetReady with score 0") {
    Game g(1, 5);
    CHECK(g.state() == State::GetReady);
    CHECK(g.score() == 0);
    CHECK(g.best() == 5);
    CHECK_FALSE(g.new_best());
    CHECK(g.pipes().pipes().empty());
}

TEST_CASE("game: negative best is clamped to 0") {
    Game g(1, -3);
    CHECK(g.best() == 0);
}

TEST_CASE("game: GetReady ignores gravity and has no pipes") {
    Game g(1, 0);
    for (int i = 0; i < 360; ++i) {
        g.tick(cfg::kDt);
        CHECK(std::fabs(g.bird().y - cfg::kBirdStartY) <= cfg::kBobAmplitude + 0.001f);
    }
    CHECK(g.state() == State::GetReady);
    CHECK(g.pipes().pipes().empty());
    CHECK(g.bird().vy == 0.0f);
}

TEST_CASE("game: press in GetReady starts Playing with flap event") {
    Game g(1, 0);
    g.press();
    CHECK(g.state() == State::Playing);
    const Events e = g.take_events();
    CHECK(e.flapped);
    CHECK(g.bird().vy < 0.0f);
    CHECK_FALSE(g.take_events().flapped);
}

TEST_CASE("game: falling bird hits ground once and reaches GameOver once") {
    Game g(1, 0);
    g.press();
    g.take_events();
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    CHECK(g.state() == State::GameOver);
    CHECK(hits == 1);
    CHECK(overs == 1);
    CHECK(bird_hit_ground(g.bird()));
}

// Hover near the ceiling zone so the bird flies into the first pipe.
// Stops as soon as the state leaves Playing.
void fly_into_pipe(Game& g) {
    g.press();
    for (int i = 0; i < 120 * 20 && g.state() == State::Playing; ++i) {
        if (g.bird().y > 60.0f && g.bird().vy > 0.0f) {
            g.press();
        }
        g.tick(cfg::kDt);
    }
}

TEST_CASE("game: Dying ignores press") {
    Game g(3, 0);
    fly_into_pipe(g);
    REQUIRE(g.state() == State::Dying);
    g.take_events();
    const float vy = g.bird().vy;
    const float y = g.bird().y;
    g.press();
    CHECK(g.state() == State::Dying);
    CHECK_FALSE(g.take_events().flapped);
    CHECK(g.bird().vy == vy);
    CHECK(g.bird().y == y);
}

TEST_CASE("game: pipes and ground are frozen in Dying") {
    Game g(3, 0);
    fly_into_pipe(g);
    REQUIRE(g.state() == State::Dying);
    std::vector<float> xs;
    for (const Pipe& p : g.pipes().pipes()) {
        xs.push_back(p.x);
    }
    const float ground = g.ground_offset();
    int ticks = 0;
    while (g.state() == State::Dying && ticks < 1200) {
        g.tick(cfg::kDt);
        ++ticks;
        std::size_t i = 0;
        for (const Pipe& p : g.pipes().pipes()) {
            REQUIRE(i < xs.size());
            CHECK(p.x == xs[i]);
            ++i;
        }
        CHECK(g.ground_offset() == ground);
    }
    CHECK(ticks > 0);
}

TEST_CASE("game: pipe hit enters Dying then GameOver") {
    // Fly level into the first pipe: flap just enough to hover near the ceiling zone.
    Game g(3, 0);
    g.press();
    bool saw_dying = false;
    for (int i = 0; i < 120 * 20 && g.state() != State::GameOver; ++i) {
        // Keep the bird near y=60 (above any gap's pipe) by flapping when low.
        if (g.state() == State::Playing && g.bird().y > 60.0f && g.bird().vy > 0.0f) {
            g.press();
        }
        g.tick(cfg::kDt);
        if (g.state() == State::Dying) {
            saw_dying = true;
        }
    }
    CHECK(g.state() == State::GameOver);
    CHECK(saw_dying);
}

TEST_CASE("game: GameOver ignores press before delay and restarts after") {
    Game g(1, 0);
    g.press();
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    REQUIRE(g.state() == State::GameOver);

    g.press();
    CHECK(g.state() == State::GameOver);

    wait_in_state(g, cfg::kGameOverInputDelay);
    g.press();
    CHECK(g.state() == State::GetReady);
    CHECK(g.score() == 0);
    CHECK_FALSE(g.new_best());
    CHECK(g.bird().x == cfg::kBirdX);
    CHECK(g.bird().y == cfg::kBirdStartY);
    CHECK(g.pipes().pipes().empty());
}

TEST_CASE("game: autopilot scores 10+ in 30 s and events match score") {
    Game g(1, 0);
    int events = 0;
    const int score = run_autopilot(g, 30.0f, &events);
    CHECK(score >= 10);
    CHECK(events == score);
    CHECK(g.state() == State::Playing);
}

TEST_CASE("game: new best set when beating old best") {
    Game g(1, 0);
    run_autopilot(g, 15.0f);
    REQUIRE(g.score() > 0);
    // Stop flapping: bird dies.
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    CHECK(g.state() == State::GameOver);
    CHECK(g.new_best());
    CHECK(g.best() == g.score());
}

TEST_CASE("game: new best not set when score does not beat best") {
    Game g(1, 1000);
    run_autopilot(g, 15.0f);
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    CHECK(g.state() == State::GameOver);
    CHECK_FALSE(g.new_best());
    CHECK(g.best() == 1000);
}

TEST_CASE("game: best equal to score is not a new best") {
    Game probe(1, 0);
    run_autopilot(probe, 15.0f);
    const int s = probe.score();
    REQUIRE(s > 0);
    Game g(1, s);
    run_autopilot(g, 15.0f);
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    CHECK(g.score() == s);
    CHECK_FALSE(g.new_best());
}

TEST_CASE("game: best never decreases across runs") {
    Game g(2, 4);
    int prev = g.best();
    for (int run = 0; run < 4; ++run) {
        g.press();
        run_autopilot(g, run * 5.0f + 1.0f);
        int hits = 0;
        int overs = 0;
        fall_to_game_over(g, hits, overs);
        CHECK(g.best() >= prev);
        prev = g.best();
        wait_in_state(g, cfg::kGameOverInputDelay);
        g.press();
        CHECK(g.state() == State::GetReady);
    }
}

TEST_CASE("game: runs after restart use different gap sequences") {
    Game g(1, 0);
    auto first_gap = [&]() {
        g.press();
        for (int i = 0; i < 120 * 5 && g.pipes().pipes().empty(); ++i) {
            if (autopilot_should_flap(g.bird(), g.pipes())) {
                g.press();
            }
            g.tick(cfg::kDt);
        }
        REQUIRE_FALSE(g.pipes().pipes().empty());
        const Gap gap = g.pipes().pipes().front().gap;
        int hits = 0;
        int overs = 0;
        fall_to_game_over(g, hits, overs);
        wait_in_state(g, cfg::kGameOverInputDelay);
        g.press();
        return gap;
    };
    const Gap a = first_gap();
    const Gap b = first_gap();
    const Gap c = first_gap();
    const bool all_same = a.gap_top == b.gap_top && b.gap_top == c.gap_top && a.gap_h == b.gap_h &&
                          b.gap_h == c.gap_h;
    CHECK_FALSE(all_same);
}

TEST_CASE("game: restarts are reproducible for the same seed") {
    auto play = []() {
        Game g(9, 0);
        g.press();
        run_autopilot(g, 5.0f);
        int hits = 0;
        int overs = 0;
        fall_to_game_over(g, hits, overs);
        wait_in_state(g, cfg::kGameOverInputDelay);
        g.press();
        g.press();
        run_autopilot(g, 10.0f);
        return g.score();
    };
    CHECK(play() == play());
}

TEST_CASE("game: ground offset stays below 24 px and freezes when dead") {
    Game g(1, 0);
    g.press();
    for (int i = 0; i < 2000 && g.state() == State::Playing; ++i) {
        if (autopilot_should_flap(g.bird(), g.pipes())) {
            g.press();
        }
        g.tick(cfg::kDt);
        CHECK(g.ground_offset() >= 0.0f);
        CHECK(g.ground_offset() < 24.0f);
    }
    int hits = 0;
    int overs = 0;
    fall_to_game_over(g, hits, overs);
    const float frozen = g.ground_offset();
    wait_in_state(g, 1.0f);
    CHECK(g.ground_offset() == frozen);
}

TEST_CASE("game: fixed-dt simulation is deterministic") {
    Game a(77, 3);
    Game b(77, 3);
    for (int i = 0; i < 120 * 20; ++i) {
        const bool flap_a = a.state() == State::GetReady ||
                            (a.state() == State::Playing && autopilot_should_flap(a.bird(), a.pipes()));
        const bool flap_b = b.state() == State::GetReady ||
                            (b.state() == State::Playing && autopilot_should_flap(b.bird(), b.pipes()));
        if (flap_a) {
            a.press();
        }
        if (flap_b) {
            b.press();
        }
        a.tick(cfg::kDt);
        b.tick(cfg::kDt);
    }
    CHECK(a.state() == b.state());
    CHECK(a.score() == b.score());
    CHECK(a.bird().y == b.bird().y);
    CHECK(a.bird().vy == b.bird().vy);
    CHECK(a.ground_offset() == b.ground_offset());
    CHECK(a.pipes().pipes().size() == b.pipes().pipes().size());
}
