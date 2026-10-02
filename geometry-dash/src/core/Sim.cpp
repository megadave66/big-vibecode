#include "core/Sim.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/PhysicsConstants.h"

namespace gd {

using namespace phys;

namespace detail {

struct Collider {
    Aabb box;
    bool hazard = false;  // spike hitbox (outer player box); else solid (inner box kills)
};

struct PortalTrigger {
    double trigger = 0;  // player centre x that fires it
    int objectIndex = -1;
};

// Immutable per-level collision index. Colliders are bucketed by integer x column.
struct SimIndex {
    std::vector<Collider> colliders;
    std::vector<int> colStart;  // size cols + 1
    std::vector<int> colItems;
    std::vector<PortalTrigger> portals;  // sorted by trigger
    int cols = 0;
};

}  // namespace detail

namespace {

constexpr double kSize = kPlayerSize;
constexpr double kInset = (kPlayerSize - kInnerHitbox) * 0.5;
constexpr int kBufferTicks = static_cast<int>(kJumpBufferTime * kTickRate + 0.5);
constexpr double kHuge = 1.0e6;
// Visual: half a turn per full flat jump.
constexpr double kCubeSpinDegPerSec = 180.0 / kCubeAirTime;
constexpr double kPi = 3.14159265358979323846;

std::shared_ptr<const detail::SimIndex> buildIndex(const Level& level) {
    auto idx = std::make_shared<detail::SimIndex>();
    double maxX = level.endX;
    for (int i = 0; i < static_cast<int>(level.objects.size()); ++i) {
        const Object& o = level.objects[i];
        switch (o.type) {
            case ObjType::Block:
                idx->colliders.push_back({Aabb{o.x, o.y, o.w, o.h}, false});
                break;
            case ObjType::Platform:
                idx->colliders.push_back({Aabb{o.x, o.y, o.w, kPlatformHeight}, false});
                break;
            case ObjType::Spike:
                idx->colliders.push_back({Sim::spikeHitbox(o), true});
                break;
            case ObjType::PortalGravity:
            case ObjType::PortalMode:
            case ObjType::PortalSpeed:
                idx->portals.push_back({o.x + 0.5, i});
                break;
            case ObjType::EndWall:
            case ObjType::Deco:
                break;
        }
        maxX = std::max(maxX, o.x + o.w);
    }
    std::stable_sort(idx->portals.begin(), idx->portals.end(),
                     [](const auto& a, const auto& b) { return a.trigger < b.trigger; });

    idx->cols = std::max(1, static_cast<int>(std::ceil(maxX)) + 2);
    std::vector<int> counts(idx->cols, 0);
    auto colRange = [&](const Aabb& b, int& c0, int& c1) {
        c0 = std::clamp(static_cast<int>(std::floor(b.x)), 0, idx->cols - 1);
        c1 = std::clamp(static_cast<int>(std::ceil(b.right())) - 1, c0, idx->cols - 1);
    };
    for (const auto& c : idx->colliders) {
        int c0, c1;
        colRange(c.box, c0, c1);
        for (int k = c0; k <= c1; ++k) ++counts[k];
    }
    idx->colStart.assign(idx->cols + 1, 0);
    for (int k = 0; k < idx->cols; ++k) idx->colStart[k + 1] = idx->colStart[k] + counts[k];
    idx->colItems.assign(idx->colStart.back(), 0);
    std::vector<int> fill(idx->colStart.begin(), idx->colStart.end() - 1);
    for (int i = 0; i < static_cast<int>(idx->colliders.size()); ++i) {
        int c0, c1;
        colRange(idx->colliders[i].box, c0, c1);
        for (int k = c0; k <= c1; ++k) idx->colItems[fill[k]++] = i;
    }
    return idx;
}

double gravitySign(Gravity g) { return g == Gravity::Down ? 1.0 : -1.0; }

double wrapDeg(double d) {
    d = std::fmod(d, 360.0);
    if (d < 0) d += 360.0;
    return d;
}

}  // namespace

Sim::Sim(const Level& level) : level_(&level), index_(buildIndex(level)) { reset(); }

void Sim::reset() {
    const Level& L = *level_;
    s_ = PlayerState{};
    s_.mode = L.startMode;
    s_.gravity = L.startGravity;
    s_.speed = L.startSpeed;
    s_.vx = speedValue(L.startSpeed);
    s_.x = 0;
    const bool down = L.startGravity == Gravity::Down;
    if (L.startMode == GameMode::Cube) {
        s_.y = down ? 0.0 : L.ceiling - kSize;
        s_.grounded = true;
    } else {
        s_.y = down ? kShipStartY : L.ceiling - kSize - kShipStartY;
        s_.grounded = false;
    }
    events_.clear();
}

double Sim::progress() const {
    const double span = level_->endX - kSize;
    if (s_.won) return 1.0;
    if (span <= 0) return 0.0;
    return std::clamp(s_.x / span, 0.0, 1.0);
}

Aabb Sim::outerBox(const PlayerState& p) { return Aabb{p.x, p.y, kSize, kSize}; }

Aabb Sim::innerBox(const PlayerState& p) {
    return Aabb{p.x + kInset, p.y + kInset, kInnerHitbox, kInnerHitbox};
}

Aabb Sim::spikeHitbox(const Object& o) {
    if (o.spikeDir == SpikeDir::Up) return Aabb{o.x + kSpikeHitX, o.y + kSpikeHitY, kSpikeHitW, kSpikeHitH};
    // dir=down: mirrored in y inside the 1x1 cell.
    return Aabb{o.x + kSpikeHitX, o.y + 1.0 - kSpikeHitY - kSpikeHitH, kSpikeHitW, kSpikeHitH};
}

template <class Fn>
void Sim::forEachNear(double x0, double x1, Fn&& fn) const {
    const detail::SimIndex& idx = *index_;
    const int c0 = std::clamp(static_cast<int>(std::floor(x0)) - 1, 0, idx.cols - 1);
    const int c1 = std::clamp(static_cast<int>(std::floor(x1)) + 1, 0, idx.cols - 1);
    if (x1 < 0 || c1 < c0) return;
    // A collider that spans several columns is visited more than once. Every caller is
    // idempotent (overlap tests, min/max), so that is harmless and cheaper than dedup.
    for (int c = c0; c <= c1; ++c)
        for (int k = idx.colStart[c]; k < idx.colStart[c + 1]; ++k) fn(idx.colliders[idx.colItems[k]]);
}

void Sim::firePortals() {
    const detail::SimIndex& idx = *index_;
    const double centre = s_.x + kSize * 0.5;
    while (s_.nextPortal < static_cast<int>(idx.portals.size()) &&
           idx.portals[s_.nextPortal].trigger <= centre) {
        const int oi = idx.portals[s_.nextPortal].objectIndex;
        const Object& o = level_->objects[oi];
        ++s_.nextPortal;
        switch (o.type) {
            case ObjType::PortalGravity:
                if (o.gravity != s_.gravity) {
                    s_.gravity = o.gravity;
                    s_.grounded = false;
                }
                break;
            case ObjType::PortalMode:
                if (o.mode != s_.mode) {
                    s_.mode = o.mode;
                    if (o.mode == GameMode::Ship) {
                        s_.vy = std::clamp(s_.vy, -kShipMaxVy, kShipMaxVy);
                        s_.grounded = false;
                    } else {
                        s_.vy = std::clamp(s_.vy, -kCubeMaxFallSpeed, kCubeMaxFallSpeed);
                    }
                    s_.bufferTicks = 0;
                }
                break;
            case ObjType::PortalSpeed:
                s_.speed = o.speed;
                s_.vx = speedValue(o.speed);
                break;
            default:
                break;
        }
        events_.push(SimEvent{SimEventType::Portal, o.type, oi});
    }
}

// Landing / sliding resolution. Uses the previous y: the player snaps onto a surface only if
// last tick its box was no deeper than kLandSnap past that surface (i.e. its inner box was
// clear of the solid). Max movement per tick (fall 26/240, run 15.6/240) is far below
// kLandSnap, so nothing tunnels.
void Sim::resolveVertical(double prevY, double vyBefore) {
    const double gs = gravitySign(s_.gravity);
    const double vAgainst = vyBefore * gs;  // > 0: moving away from the feet side
    const bool ship = s_.mode == GameMode::Ship;
    const double ceil = level_->ceiling;
    const Aabb ground{s_.x - 1.0, -kHuge, kSize + 2.0, kHuge};
    const Aabb roof{s_.x - 1.0, ceil, kSize + 2.0, kHuge};

    // Feet side: the surface gravity pulls toward.
    {
        const Aabb outer = outerBox(s_);
        bool hit = false;
        double best = 0;
        auto test = [&](const Aabb& b) {
            if (!overlaps(outer, b)) return;
            if (vAgainst > 0) return;
            if (gs > 0) {
                if (prevY >= b.top() - kLandSnap) {
                    best = hit ? std::max(best, b.top()) : b.top();
                    hit = true;
                }
            } else {
                if (prevY + kSize <= b.y + kLandSnap) {
                    best = hit ? std::min(best, b.y - kSize) : b.y - kSize;
                    hit = true;
                }
            }
        };
        test(ground);
        test(roof);
        forEachNear(outer.x, outer.right(), [&](const detail::Collider& c) {
            if (!c.hazard) test(c.box);
        });
        if (hit) {
            s_.y = best;
            s_.vy = 0;
            s_.grounded = true;
        }
    }

    // Head side: only the ship slides along it.
    if (ship) {
        const Aabb outer = outerBox(s_);
        bool hit = false;
        double best = 0;
        auto test = [&](const Aabb& b) {
            if (!overlaps(outer, b)) return;
            if (vAgainst < 0) return;
            if (gs > 0) {
                if (prevY + kSize <= b.y + kLandSnap) {
                    best = hit ? std::min(best, b.y - kSize) : b.y - kSize;
                    hit = true;
                }
            } else {
                if (prevY >= b.top() - kLandSnap) {
                    best = hit ? std::max(best, b.top()) : b.top();
                    hit = true;
                }
            }
        };
        test(ground);
        test(roof);
        forEachNear(outer.x, outer.right(), [&](const detail::Collider& c) {
            if (!c.hazard) test(c.box);
        });
        if (hit) {
            s_.y = best;
            s_.vy = 0;
        }
    }
}

bool Sim::touchesDeadly() const {
    const Aabb outer = outerBox(s_);
    const Aabb inner = innerBox(s_);
    if (inner.y < 0.0 || inner.top() > level_->ceiling) return true;
    bool dead = false;
    forEachNear(outer.x, outer.right(), [&](const detail::Collider& c) {
        if (dead) return;
        dead = c.hazard ? overlaps(outer, c.box) : overlaps(inner, c.box);
    });
    return dead;
}

void Sim::step(bool held) {
    events_.clear();
    if (s_.dead || s_.won) return;

    const bool pressed = held && !s_.prevHeld;
    s_.prevHeld = held;
    if (pressed) s_.bufferTicks = kBufferTicks;

    const double prevY = s_.y;
    const bool wasGrounded = s_.grounded;
    double gs = gravitySign(s_.gravity);

    // 1. Vertical velocity and position (trapezoid rule: exact for constant acceleration).
    if (s_.mode == GameMode::Cube) {
        if (s_.grounded && (held || s_.bufferTicks > 0)) {
            s_.vy = kCubeJumpVelocity * gs;
            s_.grounded = false;
            s_.bufferTicks = 0;
            events_.push(SimEvent{SimEventType::Jump});
        }
        const double v0 = s_.vy;
        double v1 = v0 - kCubeGravity * gs * kDt;
        if (v1 * gs < -kCubeMaxFallSpeed) v1 = -kCubeMaxFallSpeed * gs;
        s_.y += 0.5 * (v0 + v1) * kDt;
        s_.vy = v1;
    } else {
        s_.bufferTicks = 0;
        const double a = held ? kShipLift * gs : -kShipGravity * gs;
        const double v0 = s_.vy;
        const double v1 = std::clamp(v0 + a * kDt, -kShipMaxVy, kShipMaxVy);
        s_.y += 0.5 * (v0 + v1) * kDt;
        s_.vy = v1;
    }

    // 2. Horizontal move (constant speed).
    s_.x += s_.vx * kDt;

    // 3. Column-trigger portals.
    firePortals();
    gs = gravitySign(s_.gravity);

    // 4. Landing / sliding.
    const double vyBefore = s_.vy;
    s_.grounded = false;
    resolveVertical(prevY, vyBefore);
    if (s_.grounded && !wasGrounded) events_.push(SimEvent{SimEventType::Land});

    // 5. Visual rotation.
    if (s_.mode == GameMode::Cube) {
        if (s_.grounded) {
            s_.rotation = wrapDeg(std::round(s_.rotation / 90.0) * 90.0);
        } else {
            s_.rotation = wrapDeg(s_.rotation - gs * kCubeSpinDegPerSec * kDt);
        }
    } else {
        s_.rotation = wrapDeg(std::atan2(s_.vy, s_.vx) * 180.0 / kPi);
    }

    // 6. Death, then win.
    if (!invincible_ && touchesDeadly()) {
        s_.dead = true;
        events_.push(SimEvent{SimEventType::Death});
    } else if (s_.x + kSize >= level_->endX) {
        s_.won = true;
        events_.push(SimEvent{SimEventType::Win});
    }

    if (s_.bufferTicks > 0) --s_.bufferTicks;
    ++s_.tick;
}

}  // namespace gd
