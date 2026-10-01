#include "pipes.hpp"

#include <algorithm>

#include "config.hpp"

namespace flappy {

namespace {

float uniform(std::mt19937& rng, float lo, float hi) {
    if (hi <= lo) {
        return lo;
    }
    std::uniform_real_distribution<float> dist(lo, hi);
    return std::clamp(dist(rng), lo, hi);
}

}  // namespace

GapGenerator::GapGenerator(std::uint32_t seed) : rng_(seed) {}

void GapGenerator::reset(std::uint32_t seed) {
    rng_.seed(seed);
    prev_center_ = 0.0f;
    has_prev_ = false;
}

Gap GapGenerator::next() {
    const float h = uniform(rng_, config::kGapMin, config::kGapMax);
    const float lo = config::kGapMarginTop + h / 2.0f;
    const float hi = config::kGroundY - config::kGapMarginBottom - h / 2.0f;

    float c_lo = lo;
    float c_hi = hi;
    if (has_prev_) {
        c_lo = std::max(lo, prev_center_ - config::kMaxGapCenterDelta);
        c_hi = std::min(hi, prev_center_ + config::kMaxGapCenterDelta);
    } else {
        const float mid = (lo + hi) / 2.0f;
        c_lo = std::max(lo, mid - config::kMaxGapCenterDelta / 2.0f);
        c_hi = std::min(hi, mid + config::kMaxGapCenterDelta / 2.0f);
    }
    const float center = uniform(rng_, c_lo, c_hi);
    has_prev_ = true;
    prev_center_ = center;
    return Gap{center - h / 2.0f, h};
}

Rect pipe_top_rect(const Pipe& p) {
    return Rect{p.x, 0.0f, config::kPipeWidth, p.gap.gap_top};
}

Rect pipe_bottom_rect(const Pipe& p) {
    const float top = p.gap.gap_top + p.gap.gap_h;
    return Rect{p.x, top, config::kPipeWidth, config::kGroundY - top};
}

PipeField::PipeField(std::uint32_t seed) : gen_(seed) {
    reset(seed);
}

void PipeField::reset(std::uint32_t seed) {
    gen_.reset(seed);
    pipes_.clear();
    next_spawn_x_ = config::kFirstPipeX;
}

void PipeField::update(float dt) {
    const float dx = config::kPipeSpeed * dt;
    for (Pipe& p : pipes_) {
        p.x -= dx;
    }
    next_spawn_x_ -= dx;

    const float spawn_limit = static_cast<float>(config::kWorldWidth) + config::kPipeWidth;
    while (next_spawn_x_ < spawn_limit) {
        pipes_.push_back(Pipe{next_spawn_x_, gen_.next(), false});
        next_spawn_x_ += config::kPipeSpacing;
    }

    pipes_.erase(std::remove_if(pipes_.begin(), pipes_.end(),
                                [](const Pipe& p) { return p.x + config::kPipeWidth < 0.0f; }),
                 pipes_.end());
}

int PipeField::collect_score(float bird_x) {
    int n = 0;
    for (Pipe& p : pipes_) {
        if (!p.scored && p.x + config::kPipeWidth < bird_x) {
            p.scored = true;
            ++n;
        }
    }
    return n;
}

bool PipeField::collides(const Rect& box) const {
    for (const Pipe& p : pipes_) {
        if (intersects(box, pipe_top_rect(p)) || intersects(box, pipe_bottom_rect(p))) {
            return true;
        }
    }
    return false;
}

}  // namespace flappy
