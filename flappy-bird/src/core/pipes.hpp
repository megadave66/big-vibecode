// Pipes: fair gap generation, scrolling, spawning, scoring, collision.
// Pure logic, no SDL. All constants come from config.hpp.
#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "collision.hpp"

namespace flappy {

// A vertical gap: gap_top is the y of the gap's top edge, gap_h its height.
struct Gap {
    float gap_top = 0.0f;
    float gap_h = 0.0f;
};

// Deterministic, bounded gap generator.
// Guarantees for every Gap returned by next():
//   kGapMin <= gap_h <= kGapMax
//   gap_top >= kGapMarginTop
//   gap_top + gap_h <= kGroundY - kGapMarginBottom
//   |centre(n) - centre(n-1)| <= kMaxGapCenterDelta  (for n >= 1)
// The first gap after construction/reset is centred near the middle of the
// play area (within kMaxGapCenterDelta / 2 of it) so the opening is fair.
class GapGenerator {
public:
    explicit GapGenerator(std::uint32_t seed);
    void reset(std::uint32_t seed);
    Gap next();

private:
    std::mt19937 rng_;
    float prev_center_ = 0.0f;
    bool has_prev_ = false;
};

struct Pipe {
    float x = 0.0f;       // left edge (world px)
    Gap gap;
    bool scored = false;  // true once the bird has passed this pipe
};

// Top pipe: from y=0 down to gap_top. Bottom pipe: from gap bottom to kGroundY.
Rect pipe_top_rect(const Pipe& p);
Rect pipe_bottom_rect(const Pipe& p);

class PipeField {
public:
    explicit PipeField(std::uint32_t seed);

    // Remove all pipes, reseed, next spawn at kFirstPipeX.
    void reset(std::uint32_t seed);

    // Scroll all pipes left by kPipeSpeed*dt. Spawn a pipe once the spawn
    // point (starts at kFirstPipeX, scrolls with the field) is within
    // kPipeWidth of the right screen edge, so pipes always appear off-screen.
    // Consecutive left edges are exactly kPipeSpacing apart.
    // Remove pipes whose right edge is < 0.
    void update(float dt);

    // Mark every unscored pipe whose right edge is < bird_x as scored.
    // Returns how many pipes were newly scored this call (usually 0 or 1).
    int collect_score(float bird_x);

    // True if box overlaps any top or bottom pipe rect.
    bool collides(const Rect& box) const;

    const std::vector<Pipe>& pipes() const { return pipes_; }

private:
    GapGenerator gen_;
    std::vector<Pipe> pipes_;
    float next_spawn_x_ = 0.0f;  // x where the next pipe would sit, scrolls with the field
};

}  // namespace flappy
