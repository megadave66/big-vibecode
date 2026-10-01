// Simple scripted player used by the headless smoke test, the screenshot
// debug mode, and the "gaps are passable" unit test. Pure logic, no SDL.
#pragma once

#include "bird.hpp"
#include "pipes.hpp"

namespace flappy {

// Returns true when a flap now keeps the bird on course for the next gap
// (the first pipe whose right edge is still ahead of the bird's left edge).
// With no pipe ahead it holds the bird near the middle of the play area.
// Must never flap when the flap would push the hitbox through the ceiling.
bool autopilot_should_flap(const Bird& bird, const PipeField& pipes);

}  // namespace flappy
