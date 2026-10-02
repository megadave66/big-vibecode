#include <doctest/doctest.h>

#include "core/Particles.h"

using namespace gd;

TEST_CASE("particles: xorshift is deterministic and never zero") {
    XorShift32 a(7), b(7), c(8);
    bool differs = false;
    for (int i = 0; i < 1000; ++i) {
        const auto va = a.next();
        CHECK(va == b.next());
        CHECK(va != 0u);
        if (va != c.next()) differs = true;
    }
    CHECK(differs);
    XorShift32 z(0);  // zero seed is replaced
    CHECK(z.next() != 0u);
}

TEST_CASE("particles: uniform stays in range") {
    XorShift32 r(99);
    for (int i = 0; i < 5000; ++i) {
        const double u = r.uniform();
        CHECK(u >= 0.0);
        CHECK(u < 1.0);
        const double v = r.range(-3.0, 5.0);
        CHECK(v >= -3.0);
        CHECK(v < 5.0);
    }
}

TEST_CASE("particles: same seed and calls give identical systems") {
    ParticleSystem a(5), b(5);
    for (auto* ps : {&a, &b}) {
        ps->burst(1.0, 2.0, 30, Color{255, 0, 0}, 2.0, 8.0, 0.3, 0.7, 0.2, -20.0);
        for (int i = 0; i < 20; ++i) ps->update(1.0 / 240.0);
    }
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a.particles()[i].x == b.particles()[i].x);
        CHECK(a.particles()[i].y == b.particles()[i].y);
    }
}

TEST_CASE("particles: they expire and the list empties") {
    ParticleSystem ps(1);
    ps.burst(0, 0, 40, Color{1, 2, 3}, 1.0, 2.0, 0.2, 0.4, 0.2, 0.0);
    CHECK(ps.size() == 40);
    for (int i = 0; i < 240; ++i) ps.update(1.0 / 240.0);  // 1 s
    CHECK(ps.size() == 0);
}

TEST_CASE("particles: gravity pulls down, alpha fades, capacity is capped") {
    ParticleSystem ps(1);
    Particle p;
    p.life = p.maxLife = 1.0;
    p.ay = -10.0;
    p.alpha = 200;
    ps.emit(p);
    ps.update(0.5);
    CHECK(ps.particles()[0].vy == doctest::Approx(-5.0));
    CHECK(ps.particles()[0].y < 0.0);
    CHECK(ps.particles()[0].alphaNow() == doctest::Approx(100).epsilon(0.02));
    for (std::size_t i = 0; i < ParticleSystem::kCapacity + 50; ++i) ps.emit(p);
    CHECK(ps.size() == ParticleSystem::kCapacity);
}

TEST_CASE("particles: ring grows from d0 to d1") {
    ParticleSystem ps(1);
    ps.ring(0, 0, Color{9, 9, 9}, 0.5, 3.5, 1.0);
    CHECK(ps.particles()[0].size() == doctest::Approx(0.5));
    ps.update(0.5);
    CHECK(ps.particles()[0].size() == doctest::Approx(2.0));
}
