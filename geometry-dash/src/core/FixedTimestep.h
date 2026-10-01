#pragma once
// Fixed-step accumulator. Feed real elapsed seconds; get back how many sim ticks to run.

#include <algorithm>

#include "core/PhysicsConstants.h"

namespace gd {

class FixedTimestep {
public:
    static constexpr double kMaxFrameTime = 0.25;  // spiral-of-death cap, seconds

    explicit FixedTimestep(double tickRate = phys::kTickRate) : step_(1.0 / tickRate) {}

    // Add real elapsed time (seconds) and return the number of ticks to run now.
    int advance(double realDt) {
        if (realDt < 0.0) realDt = 0.0;
        realDt = std::min(realDt, kMaxFrameTime);
        acc_ += realDt;
        int ticks = 0;
        while (acc_ >= step_) {
            acc_ -= step_;
            ++ticks;
        }
        return ticks;
    }

    // Fraction (0..1) of a tick left in the accumulator. Use to interpolate rendering.
    double alpha() const { return acc_ / step_; }
    double step() const { return step_; }
    void reset() { acc_ = 0.0; }

private:
    double step_;
    double acc_ = 0.0;
};

}  // namespace gd
