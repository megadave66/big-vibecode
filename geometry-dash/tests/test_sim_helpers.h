#pragma once
// Shared helpers for the physics test suites (synthetic levels built in C++).

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/Level.h"
#include "core/PhysicsConstants.h"
#include "core/Sim.h"

namespace simtest {

using namespace gd;

inline Object block(double x, double y, double w = 1, double h = 1) {
    Object o;
    o.type = ObjType::Block;
    o.x = x, o.y = y, o.w = w, o.h = h;
    return o;
}
inline Object platform(double x, double y, double w = 1) {
    Object o;
    o.type = ObjType::Platform;
    o.x = x, o.y = y, o.w = w, o.h = phys::kPlatformHeight;
    return o;
}
inline Object spike(double x, double y = 0, SpikeDir d = SpikeDir::Up) {
    Object o;
    o.type = ObjType::Spike;
    o.x = x, o.y = y;
    o.spikeDir = d;
    return o;
}
inline Object gravityPortal(double x, Gravity g) {
    Object o;
    o.type = ObjType::PortalGravity;
    o.x = x, o.h = phys::kPortalDrawHeight;
    o.gravity = g;
    return o;
}
inline Object modePortal(double x, GameMode m) {
    Object o;
    o.type = ObjType::PortalMode;
    o.x = x, o.h = phys::kPortalDrawHeight;
    o.mode = m;
    return o;
}
inline Object speedPortal(double x, Speed s) {
    Object o;
    o.type = ObjType::PortalSpeed;
    o.x = x, o.h = phys::kPortalDrawHeight;
    o.speed = s;
    return o;
}

inline Level makeLevel(double endX, std::vector<Object> objs = {}, GameMode mode = GameMode::Cube,
                       Speed speed = Speed::Normal, Gravity gravity = Gravity::Down,
                       double ceiling = phys::kDefaultCeiling) {
    Level L;
    L.id = 0;
    L.name = "test";
    L.startMode = mode;
    L.startSpeed = speed;
    L.startGravity = gravity;
    L.ceiling = ceiling;
    Object end;
    end.type = ObjType::EndWall;
    end.x = endX;
    end.h = ceiling;
    objs.push_back(end);
    std::stable_sort(objs.begin(), objs.end(), [](const Object& a, const Object& b) { return a.x < b.x; });
    L.objects = objs;
    L.endX = endX;
    return L;
}

// Run until dead, won, or maxTicks. `input(sim)` returns held for the next tick.
inline void run(Sim& sim, int maxTicks, const std::function<bool(const Sim&)>& input) {
    while (!sim.dead() && !sim.won() && sim.tick() < maxTicks) sim.step(input(sim));
}

// Single one-tick press at tick `t`, never held otherwise.
inline std::function<bool(const Sim&)> tapAt(int t) {
    return [t](const Sim& s) { return s.tick() == t; };
}

inline bool sameState(const PlayerState& a, const PlayerState& b) {
    return a.x == b.x && a.y == b.y && a.vy == b.vy && a.vx == b.vx && a.mode == b.mode &&
           a.gravity == b.gravity && a.speed == b.speed && a.grounded == b.grounded &&
           a.dead == b.dead && a.won == b.won && a.prevHeld == b.prevHeld &&
           a.bufferTicks == b.bufferTicks && a.nextPortal == b.nextPortal &&
           a.rotation == b.rotation && a.tick == b.tick;
}

// Deterministic pseudo-random held script (blocks of held/released runs).
inline std::vector<bool> lcgScript(int n, std::uint32_t seed) {
    std::vector<bool> out(n);
    std::uint32_t s = seed;
    bool cur = false;
    int left = 0;
    for (int i = 0; i < n; ++i) {
        if (left == 0) {
            s = s * 1664525u + 1013904223u;
            cur = !cur;
            left = 1 + static_cast<int>((s >> 16) % 90u);
        }
        out[i] = cur;
        --left;
    }
    return out;
}

inline double speedOf(Speed s) { return speedValue(s); }

}  // namespace simtest
