#pragma once
// THE single source of physics constants. Shared by the sim, the validator, the solver,
// the game and the tests. Units: blocks, seconds. y grows up.
// Owner: physics section. Values may be tuned; names must stay stable (validator uses them).

#include <cmath>

namespace gd::phys {

// ---- Timing ----
inline constexpr int kTickRate = 240;                 // fixed sim ticks per second
inline constexpr double kDt = 1.0 / kTickRate;        // seconds per tick

// ---- Horizontal speeds (blocks / second) ----
inline constexpr double kSpeedSlow = 8.372;
inline constexpr double kSpeedNormal = 10.386;
inline constexpr double kSpeedFast = 12.914;
inline constexpr double kSpeedFaster = 15.6;

// ---- World ----
inline constexpr double kDefaultCeiling = 12.0;
inline constexpr double kMinCeiling = 6.0;
inline constexpr double kMaxCeiling = 20.0;
inline constexpr double kPlatformHeight = 0.5;
inline constexpr double kPortalDrawHeight = 3.0;

// ---- Player ----
inline constexpr double kPlayerSize = 1.0;            // outer hitbox (hazards, landing)
inline constexpr double kInnerHitbox = 0.3;           // centred inner square: overlap with a solid = death
inline constexpr double kShipStartY = 1.0;

// ---- Cube ----
inline constexpr double kCubeGravity = 86.0;          // blocks/s^2
inline constexpr double kCubeJumpVelocity = 19.6;     // blocks/s at take-off
inline constexpr double kCubeMaxFallSpeed = 26.0;     // terminal speed
inline constexpr double kJumpBufferTime = 0.10;       // press this long before landing still jumps
// Landing tolerance: the player snaps onto a surface it moves toward if, on the previous tick,
// its box was at most this deep past that surface. Equal to the inner-hitbox inset, so the
// player snaps up onto a corner exactly when its inner box is still clear of the solid.
inline constexpr double kLandSnap = (kPlayerSize - kInnerHitbox) * 0.5;  // 0.35

// ---- Ship ----
inline constexpr double kShipGravity = 36.0;          // accel toward gravity when not held
inline constexpr double kShipLift = 36.0;             // accel against gravity when held
inline constexpr double kShipMaxVy = 9.0;             // |vy| clamp

// ---- Spike hitbox (for dir=up, relative to the spike's cell; dir=down mirrors in y) ----
inline constexpr double kSpikeHitX = 0.30;
inline constexpr double kSpikeHitW = 0.40;
inline constexpr double kSpikeHitY = 0.00;
inline constexpr double kSpikeHitH = 0.55;

// ---- Derived (used by validator and authoring) ----
inline constexpr double kCubeJumpApex =
    kCubeJumpVelocity * kCubeJumpVelocity / (2.0 * kCubeGravity);     // ~2.23 blocks
inline constexpr double kCubeAirTime = 2.0 * kCubeJumpVelocity / kCubeGravity;  // ~0.456 s
inline constexpr double kCubeMaxClimb = kCubeJumpApex - 0.25;            // safe step-up height

inline double cubeJumpLength(double speed) { return kCubeAirTime * speed; }

// Time the cube's bottom stays above height h during a flat jump.
inline double cubeTimeAbove(double h) {
    const double v = kCubeJumpVelocity, g = kCubeGravity;
    const double disc = v * v - 2.0 * g * h;
    if (disc <= 0.0) return 0.0;
    return 2.0 * std::sqrt(disc) / g;
}

// Longest run of adjacent floor spikes (in blocks) a cube can clear at this speed, with margin.
inline double cubeMaxSpikeRun(double speed) {
    const double clear = cubeTimeAbove(kSpikeHitY + kSpikeHitH) * speed;  // horizontal distance
    // run of N spikes needs (N - 2*kSpikeHitX) + kPlayerSize of travel above the hitbox
    return std::floor(clear - kPlayerSize + 2.0 * kSpikeHitX - 0.25);
}

inline constexpr double kShipMinGap = 2.5;            // min free vertical corridor in ship sections
inline constexpr double kShipMaxSlope = 0.5;          // max corridor centre change per block of x

inline constexpr double kLevelMinSeconds = 30.0;
inline constexpr double kLevelMaxSeconds = 60.0;
inline constexpr double kStartClearBlocks = 8.0;

}  // namespace gd::phys
