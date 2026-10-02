#pragma once
// App state enum + transition table. Menus extend it. Dead is the instant-restart state:
// it lasts a short moment, then goes straight back to Playing.

#include <array>
#include <cstddef>

namespace gd {

enum class AppState { Boot, MainMenu, LevelSelect, Playing, Paused, Dead, Complete, Quit };

inline constexpr std::size_t kAppStateCount = 8;

inline const char* stateName(AppState s) {
    switch (s) {
        case AppState::Boot: return "Boot";
        case AppState::MainMenu: return "MainMenu";
        case AppState::LevelSelect: return "LevelSelect";
        case AppState::Playing: return "Playing";
        case AppState::Paused: return "Paused";
        case AppState::Dead: return "Dead";
        case AppState::Complete: return "Complete";
        case AppState::Quit: return "Quit";
    }
    return "?";
}

// Allowed transitions. Row = from, column = to. Quit is terminal and reachable from any state
// except itself.
inline bool canTransition(AppState from, AppState to) {
    using S = AppState;
    if (from == S::Quit) return false;
    if (to == S::Quit) return true;
    switch (from) {
        case S::Boot: return to == S::MainMenu;
        case S::MainMenu: return to == S::LevelSelect || to == S::Playing;
        case S::LevelSelect: return to == S::MainMenu || to == S::Playing;
        case S::Playing: return to == S::Paused || to == S::Dead || to == S::Complete;
        case S::Paused: return to == S::Playing || to == S::LevelSelect || to == S::MainMenu;
        case S::Dead: return to == S::Playing || to == S::LevelSelect || to == S::MainMenu;
        case S::Complete: return to == S::Playing || to == S::LevelSelect || to == S::MainMenu;
        case S::Quit: return false;
    }
    return false;
}

// Applies the transition when legal. Returns true if the state changed.
inline bool transition(AppState& current, AppState to) {
    if (!canTransition(current, to)) return false;
    current = to;
    return true;
}

}  // namespace gd
