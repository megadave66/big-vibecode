#pragma once
// Small interpolation helpers for rendering between two sim ticks. Pure math, no SDL.

#include <cmath>

namespace gd {

inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Shortest signed difference b - a in degrees, in (-180, 180].
inline double angleDiffDeg(double a, double b) {
    double d = std::fmod(b - a, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
}

// Interpolate angles along the short way round. Result is not wrapped (may be outside 0..360).
inline double lerpAngleDeg(double a, double b, double t) { return a + angleDiffDeg(a, b) * t; }

inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace gd
