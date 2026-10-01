#pragma once

#include "Options.hpp"
#include "Scene.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <string>

namespace bac::app {

inline constexpr int kLogicalWidth = 1280;
inline constexpr int kLogicalHeight = 720;
inline constexpr double kFixedDt = 1.0 / 60.0;

// Fonts loaded at start-up. Any pointer may be null (no font found):
// scenes must then skip text, never crash.
struct Fonts {
  TTF_Font* small = nullptr;   // Lato Regular 16
  TTF_Font* body = nullptr;    // Lato Regular 22
  TTF_Font* bold = nullptr;    // Lato Bold 28
  TTF_Font* title = nullptr;   // Lato Bold 52
};

// Everything a scene may use. Owned by App; scenes borrow it.
struct AppContext {
  const AppOptions* opts = nullptr;
  SDL_Renderer* renderer = nullptr;
  Fonts fonts;
  MIX_Mixer* mixer = nullptr;  // null when audio is unavailable
  std::string assetDir;        // absolute path to assets/ (no trailing slash)
};

class App {
 public:
  // Create and initialise SDL, window, renderer, fonts, mixer and the scene.
  // Returns nullptr on a fatal error (message already logged).
  static std::unique_ptr<App> create(const AppOptions& opts);
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  // Main loop. Returns the process exit code.
  int run();

 private:
  App() = default;
  void frame(bool lastFrame);
  bool saveScreenshot(const std::string& path);

  AppOptions opts_;
  SDL_Window* window_ = nullptr;
  AppContext ctx_;
  std::unique_ptr<Scene> scene_;
  bool ttfReady_ = false;
  bool mixReady_ = false;
  bool quitRequested_ = false;
  bool screenshotFailed_ = false;
};

}  // namespace bac::app
