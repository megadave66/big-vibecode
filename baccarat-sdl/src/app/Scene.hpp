#pragma once

#include <SDL3/SDL.h>

namespace bac::app {

struct AppContext;

// One screen of the game. The App drives it with a fixed 60 Hz timestep:
// handleEvent for each input event, then update(kFixedDt), then render().
class Scene {
 public:
  virtual ~Scene() = default;
  virtual void handleEvent(const SDL_Event& event) = 0;
  virtual void update(double dt) = 0;
  virtual void render(SDL_Renderer* renderer) = 0;
  // Scenes return true to end the app (e.g. a scripted run finished).
  virtual bool wantsQuit() const { return false; }
  // Non-zero = the app exits with this code (e.g. a failed script assert).
  virtual int exitCode() const { return 0; }
};

}  // namespace bac::app
