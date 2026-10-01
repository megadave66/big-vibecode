#include "bird.hpp"

#include <algorithm>

#include "config.hpp"

namespace flappy {

namespace {

float clamp_angle(float a) {
    return std::clamp(a, config::kMaxUpAngle, config::kMaxDownAngle);
}

}  // namespace

void bird_reset(Bird& b) {
    b = Bird{config::kBirdX, config::kBirdStartY, 0.0f, 0.0f};
}

void bird_flap(Bird& b) {
    b.vy = config::kFlapVelocity;
    b.angle = config::kMaxUpAngle;
}

void bird_update(Bird& b, float dt) {
    const float old_vy = b.vy;
    b.vy = std::min(b.vy + config::kGravity * dt, config::kMaxFallSpeed);
    // Average of old and new velocity: exact for constant gravity, so the
    // result does not depend on the step size.
    b.y += 0.5f * (old_vy + b.vy) * dt;

    if (b.vy > config::kNoseDiveVelocity) {
        b.angle += config::kRotateDownSpeed * dt;
    } else {
        b.angle = std::max(b.angle - config::kRotateDownSpeed * dt, config::kMaxUpAngle);
    }
    b.angle = clamp_angle(b.angle);
}

void bird_land(Bird& b) {
    b.y = config::kGroundY - (config::kBirdH / 2.0f - config::kBirdHitboxInset);
    b.vy = 0.0f;
    b.angle = config::kMaxDownAngle;
}

Rect bird_hitbox(const Bird& b) {
    const float inset = config::kBirdHitboxInset;
    return Rect{b.x - config::kBirdW / 2.0f + inset, b.y - config::kBirdH / 2.0f + inset,
                config::kBirdW - 2.0f * inset, config::kBirdH - 2.0f * inset};
}

bool bird_hit_ground(const Bird& b) {
    const Rect r = bird_hitbox(b);
    return r.y + r.h >= config::kGroundY;
}

bool bird_hit_ceiling(const Bird& b) {
    return bird_hitbox(b).y <= 0.0f;
}

}  // namespace flappy
