#include <doctest/doctest.h>

#include <string>

#include "core/StateMachine.h"

using gd::AppState;
using gd::canTransition;
using gd::transition;

TEST_CASE("boot goes only to the main menu (or quit)") {
    CHECK(canTransition(AppState::Boot, AppState::MainMenu));
    CHECK_FALSE(canTransition(AppState::Boot, AppState::Playing));
    CHECK(canTransition(AppState::Boot, AppState::Quit));
}

TEST_CASE("normal flow: menu -> select -> play -> pause -> play") {
    AppState s = AppState::Boot;
    CHECK(transition(s, AppState::MainMenu));
    CHECK(transition(s, AppState::LevelSelect));
    CHECK(transition(s, AppState::Playing));
    CHECK(transition(s, AppState::Paused));
    CHECK(transition(s, AppState::Playing));
    CHECK(s == AppState::Playing);
}

TEST_CASE("Dead is the instant-restart state") {
    AppState s = AppState::Playing;
    CHECK(transition(s, AppState::Dead));
    CHECK(transition(s, AppState::Playing));
    CHECK_FALSE(canTransition(AppState::Dead, AppState::Paused));
    CHECK_FALSE(canTransition(AppState::Dead, AppState::Complete));
}

TEST_CASE("illegal transitions leave the state unchanged") {
    AppState s = AppState::MainMenu;
    CHECK_FALSE(transition(s, AppState::Paused));
    CHECK(s == AppState::MainMenu);
    CHECK_FALSE(transition(s, AppState::Dead));
    CHECK_FALSE(transition(s, AppState::Complete));
    CHECK_FALSE(transition(s, AppState::Boot));
    CHECK(s == AppState::MainMenu);
}

TEST_CASE("self transitions are not allowed") {
    for (std::size_t i = 0; i < gd::kAppStateCount; ++i) {
        const auto st = static_cast<AppState>(i);
        CHECK_FALSE(canTransition(st, st));
    }
}

TEST_CASE("Quit is reachable from every other state and is terminal") {
    for (std::size_t i = 0; i < gd::kAppStateCount; ++i) {
        const auto st = static_cast<AppState>(i);
        if (st == AppState::Quit) continue;
        CHECK(canTransition(st, AppState::Quit));
    }
    for (std::size_t i = 0; i < gd::kAppStateCount; ++i)
        CHECK_FALSE(canTransition(AppState::Quit, static_cast<AppState>(i)));
}

TEST_CASE("Complete and Paused can return to menus") {
    CHECK(canTransition(AppState::Complete, AppState::LevelSelect));
    CHECK(canTransition(AppState::Complete, AppState::Playing));
    CHECK(canTransition(AppState::Paused, AppState::MainMenu));
    CHECK_FALSE(canTransition(AppState::Complete, AppState::Paused));
}

TEST_CASE("stateName names every state") {
    for (std::size_t i = 0; i < gd::kAppStateCount; ++i)
        CHECK(std::string(gd::stateName(static_cast<AppState>(i))) != "?");
}
