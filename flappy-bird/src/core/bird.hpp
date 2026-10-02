// Bird physics. Pure logic, no SDL. All constants come from config.hpp.
#pragma once

#include "collision.hpp"

namespace flappy {

struct Bird {
    float x = 0.0f;      // centre x (world px)
    float y = 0.0f;      // centre y (world px)
    float vy = 0.0f;     // vertical velocity px/s, +down
    float angle = 0.0f;  // degrees, negative = nose up, clamped to [kMaxUpAngle, kMaxDownAngle]
};

// Place the bird at (kBirdX, kBirdStartY), zero velocity and angle.
void bird_reset(Bird& b);

// Set vy = kFlapVelocity and snap angle to kMaxUpAngle.
void bird_flap(Bird& b);

// Advance one step of dt seconds: apply gravity, clamp to kMaxFallSpeed,
// integrate y, update angle (nose-up after flap, rotate down once
// vy > kNoseDiveVelocity at kRotateDownSpeed, clamp to limits).
// Does NOT clamp y to the screen; callers use bird_hit_ground/ceiling.
void bird_update(Bird& b, float dt);

// Clamp the bird so its hitbox bottom sits on kGroundY, vy = 0.
// Used while the bird rests on the ground after death.
void bird_land(Bird& b);

// Collision box: sprite box (kBirdW x kBirdH centred on x,y) shrunk by
// kBirdHitboxInset on every side. Rotation is ignored (deliberate, forgiving).
Rect bird_hitbox(const Bird& b);

// True when hitbox bottom >= kGroundY.
bool bird_hit_ground(const Bird& b);

// True when hitbox top <= 0.
bool bird_hit_ceiling(const Bird& b);

}  // namespace flappy
