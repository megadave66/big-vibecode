#pragma once
// Deterministic particle system for visual effects. Pure logic, no SDL, own RNG.
// The same seed and the same calls always give the same particles.

#include <cstdint>
#include <vector>

#include "core/Level.h"

namespace gd {

// xorshift32. Never returns state 0.
class XorShift32 {
public:
    explicit XorShift32(std::uint32_t seed = 0x9E3779B9u) { reseed(seed); }
    void reseed(std::uint32_t seed) { s_ = seed ? seed : 0x9E3779B9u; }
    std::uint32_t next() {
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return s_;
    }
    double uniform() { return (next() >> 8) * (1.0 / 16777216.0); }          // [0, 1)
    double range(double a, double b) { return a + (b - a) * uniform(); }      // [a, b)
    std::uint32_t state() const { return s_; }

private:
    std::uint32_t s_;
};

enum class ParticleShape : std::uint8_t { Square, Ring };

struct Particle {
    double x = 0, y = 0;          // world position (blocks), centre
    double vx = 0, vy = 0;        // blocks/s
    double ay = 0;                // acceleration y (blocks/s^2), negative = falls
    double life = 0, maxLife = 1; // seconds left / total
    double size0 = 0.2, size1 = 0.0;  // size (blocks) at birth / death; Ring: diameter
    double rot = 0, spin = 0;     // degrees, degrees/s
    Color color{255, 255, 255};
    std::uint8_t alpha = 255;     // alpha at birth; fades linearly to 0
    ParticleShape shape = ParticleShape::Square;
    bool additive = false;

    double t() const { return maxLife > 0 ? 1.0 - life / maxLife : 1.0; }  // 0 at birth, 1 at death
    double size() const { return size0 + (size1 - size0) * t(); }
    int alphaNow() const { return static_cast<int>(alpha * (1.0 - t())); }
};

class ParticleSystem {
public:
    static constexpr std::size_t kCapacity = 2048;

    explicit ParticleSystem(std::uint32_t seed = 12345u) : rng_(seed) {}

    void reseed(std::uint32_t seed) { rng_.reseed(seed); }
    void clear() { items_.clear(); }
    bool emit(const Particle& p);
    void update(double dt);

    const std::vector<Particle>& particles() const { return items_; }
    std::size_t size() const { return items_.size(); }
    XorShift32& rng() { return rng_; }

    // Ready-made effects.
    // Burst of squares flying out of (x, y); gravity pulls them down.
    void burst(double x, double y, int count, Color color, double speedMin, double speedMax,
               double lifeMin, double lifeMax, double size, double gravity);
    // Expanding ring (diameter grows from d0 to d1).
    void ring(double x, double y, Color color, double d0, double d1, double life, bool additive = true);
    // Single small trail square drifting backwards.
    void trail(double x, double y, Color color, double size, double life, double vx);

private:
    std::vector<Particle> items_;
    XorShift32 rng_;
};

}  // namespace gd
