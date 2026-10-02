#include "autopilot.hpp"

#include "config.hpp"

namespace flappy {

bool autopilot_should_flap(const Bird& bird, const PipeField& pipes) {
    constexpr float kCeilingMargin = 8.0f;  // keep the hitbox this far below the ceiling
    constexpr float kAimFromGapBottom = 0.15f;  // aim this fraction of the gap above its bottom
    const Rect hb = bird_hitbox(bird);

    float aim = (config::kGapMarginTop + config::kGroundY - config::kGapMarginBottom) / 2.0f;
    for (const Pipe& p : pipes.pipes()) {
        if (p.x + config::kPipeWidth >= hb.x) {
            aim = p.gap.gap_top + p.gap.gap_h - kAimFromGapBottom * p.gap.gap_h;
            break;
        }
    }

    const float apex_rise = config::kFlapVelocity * config::kFlapVelocity / (2.0f * config::kGravity);
    if (hb.y - apex_rise < kCeilingMargin) {
        return false;
    }
    return bird.y > aim && bird.vy >= 0.0f;
}

}  // namespace flappy
