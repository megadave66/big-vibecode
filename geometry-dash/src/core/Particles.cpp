#include "core/Particles.h"

#include <algorithm>
#include <cmath>

namespace gd {

bool ParticleSystem::emit(const Particle& p) {
    if (items_.size() >= kCapacity) return false;
    items_.push_back(p);
    return true;
}

void ParticleSystem::update(double dt) {
    for (Particle& p : items_) {
        p.vy += p.ay * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.rot += p.spin * dt;
        p.life -= dt;
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(), [](const Particle& p) { return p.life <= 0.0; }),
                 items_.end());
}

void ParticleSystem::burst(double x, double y, int count, Color color, double speedMin, double speedMax,
                           double lifeMin, double lifeMax, double size, double gravity) {
    constexpr double kTwoPi = 6.283185307179586;
    for (int i = 0; i < count; ++i) {
        const double a = rng_.range(0.0, kTwoPi);
        const double sp = rng_.range(speedMin, speedMax);
        Particle p;
        p.x = x;
        p.y = y;
        p.vx = std::cos(a) * sp;
        p.vy = std::sin(a) * sp;
        p.ay = gravity;
        p.maxLife = p.life = rng_.range(lifeMin, lifeMax);
        p.size0 = size * rng_.range(0.6, 1.2);
        p.size1 = p.size0 * 0.25;
        p.rot = rng_.range(0.0, 360.0);
        p.spin = rng_.range(-540.0, 540.0);
        p.color = color;
        emit(p);
    }
}

void ParticleSystem::ring(double x, double y, Color color, double d0, double d1, double life, bool additive) {
    Particle p;
    p.x = x;
    p.y = y;
    p.maxLife = p.life = life;
    p.size0 = d0;
    p.size1 = d1;
    p.color = color;
    p.alpha = 230;
    p.shape = ParticleShape::Ring;
    p.additive = additive;
    emit(p);
}

void ParticleSystem::trail(double x, double y, Color color, double size, double life, double vx) {
    Particle p;
    p.x = x + rng_.range(-0.08, 0.08);
    p.y = y + rng_.range(-0.08, 0.08);
    p.vx = vx * 0.15;
    p.vy = rng_.range(-0.4, 0.4);
    p.maxLife = p.life = life * rng_.range(0.8, 1.2);
    p.size0 = size;
    p.size1 = size * 0.1;
    p.rot = rng_.range(0.0, 90.0);
    p.spin = rng_.range(-200.0, 200.0);
    p.color = color;
    p.alpha = 200;
    emit(p);
}

}  // namespace gd
