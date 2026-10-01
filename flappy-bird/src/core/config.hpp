// Single source of truth for every gameplay constant.
// Units: world pixels (logical 288x512 canvas), seconds, degrees.
// No SDL dependency. Do not duplicate these values anywhere else.
#pragma once

namespace flappy::config {

// --- World / timing ---------------------------------------------------------
inline constexpr int kWorldWidth = 288;
inline constexpr int kWorldHeight = 512;
inline constexpr int kGroundHeight = 112;
inline constexpr float kGroundY = static_cast<float>(kWorldHeight - kGroundHeight);  // 400: top of ground
inline constexpr int kWindowScale = 2;                 // window = world * scale
inline constexpr int kTickHz = 120;                    // fixed simulation rate
inline constexpr float kDt = 1.0f / static_cast<float>(kTickHz);
inline constexpr float kMaxFrameTime = 0.25f;          // clamp to avoid spiral of death

// --- Bird -------------------------------------------------------------------
inline constexpr float kBirdX = 80.0f;                 // fixed x of bird centre
inline constexpr float kBirdStartY = 240.0f;           // y of bird centre at reset
inline constexpr float kBirdW = 34.0f;                 // sprite size
inline constexpr float kBirdH = 24.0f;
inline constexpr float kBirdHitboxInset = 4.0f;        // hitbox shrinks by this on each side
inline constexpr float kGravity = 1500.0f;             // px/s^2, +y is down
inline constexpr float kFlapVelocity = -430.0f;        // px/s, set (not added) on flap
inline constexpr float kMaxFallSpeed = 650.0f;         // terminal velocity px/s
inline constexpr float kMaxUpAngle = -25.0f;           // degrees (nose up)
inline constexpr float kMaxDownAngle = 90.0f;          // degrees (nose down)
inline constexpr float kNoseDiveVelocity = 250.0f;     // vy above which the bird starts to rotate down
inline constexpr float kRotateDownSpeed = 360.0f;      // deg/s once diving
inline constexpr float kBobAmplitude = 6.0f;           // get-ready idle bob, px
inline constexpr float kBobHz = 1.5f;

// --- Pipes ------------------------------------------------------------------
inline constexpr float kPipeWidth = 52.0f;
inline constexpr float kPipeSpeed = 120.0f;            // px/s scroll to the left
inline constexpr float kPipeSpacing = 160.0f;          // horizontal distance between pipe left edges
inline constexpr float kFirstPipeX = static_cast<float>(kWorldWidth) + 60.0f;
inline constexpr float kGapMin = 100.0f;               // vertical gap height range (inclusive)
inline constexpr float kGapMax = 125.0f;
inline constexpr float kGapMarginTop = 50.0f;          // gap never closer than this to the ceiling
inline constexpr float kGapMarginBottom = 50.0f;       // ... or to the ground (kGroundY)
inline constexpr float kMaxGapCenterDelta = 140.0f;    // max |centre change| between consecutive gaps

// --- Flow / UI --------------------------------------------------------------
inline constexpr float kGameOverInputDelay = 0.6f;     // s before a press can restart
inline constexpr float kGroundScrollSpeed = kPipeSpeed;
inline constexpr float kGroundPatternWidth = 24.0f;    // ground art repeats every 24 px (tools/gen_art.py)

}  // namespace flappy::config
