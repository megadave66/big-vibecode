#pragma once
// Deterministic fixed-step gameplay simulation. One step() = one tick of phys::kDt.
// Rules: docs/LEVEL-FORMAT.md. Constants: PhysicsConstants.h. Owner: physics section.
//
// Copy cost: PlayerState is plain data; Sim holds a shared pointer to an immutable collision
// index built once per level, so copying a Sim or taking a Snapshot is cheap.

#include <array>
#include <cstdint>
#include <memory>

#include "core/Aabb.h"
#include "core/Level.h"

namespace gd {

struct PlayerState {
    double x = 0, y = 0;          // bottom-left of the kPlayerSize box, blocks
    double vy = 0;                // blocks/s, y up
    double vx = 0;                // blocks/s, from `speed`
    GameMode mode = GameMode::Cube;
    Gravity gravity = Gravity::Down;
    Speed speed = Speed::Normal;
    bool grounded = false;        // resting on the surface gravity pulls toward
    bool dead = false;
    bool won = false;
    bool prevHeld = false;        // held state of the previous tick (press edge detection)
    int bufferTicks = 0;          // ticks left in which a buffered press still jumps
    int nextPortal = 0;           // index of the next portal trigger to fire
    double rotation = 0;          // visual only, degrees, counter-clockwise (y up), [0, 360)
    int tick = 0;                 // ticks stepped since reset
};

// Full sim state. Restoring it reproduces the run exactly.
struct Snapshot {
    PlayerState state;
};

enum class SimEventType : std::uint8_t { Jump, Land, Death, Win, Portal };

struct SimEvent {
    SimEventType type = SimEventType::Jump;
    ObjType portal = ObjType::PortalGravity;  // for Portal: which portal type fired
    int objectIndex = -1;                     // for Portal: index into level.objects
};

// Small fixed-capacity event list (no heap use, so a Sim copy stays cheap).
class SimEvents {
public:
    static constexpr int kCapacity = 16;
    void clear() { n_ = 0; }
    void push(const SimEvent& e) {
        if (n_ < kCapacity) items_[n_++] = e;
    }
    int size() const { return n_; }
    bool empty() const { return n_ == 0; }
    const SimEvent& operator[](int i) const { return items_[i]; }
    const SimEvent* begin() const { return items_.data(); }
    const SimEvent* end() const { return items_.data() + n_; }
    bool has(SimEventType t) const {
        for (const SimEvent& e : *this)
            if (e.type == t) return true;
        return false;
    }

private:
    std::array<SimEvent, kCapacity> items_{};
    int n_ = 0;
};

namespace detail {
struct SimIndex;
}

class Sim {
public:
    // `level` must outlive the Sim (and every copy of it).
    explicit Sim(const Level& level);

    void reset();
    // Advance one tick. `held` = jump/fly input is down during this tick.
    // Does nothing once dead or won.
    void step(bool held);

    const PlayerState& player() const { return s_; }
    bool dead() const { return s_.dead; }
    bool won() const { return s_.won; }
    int tick() const { return s_.tick; }
    // 0..1: player x over (endX - kPlayerSize); 1 when the end wall is reached.
    double progress() const;

    // Events from the most recent step() (cleared at the start of each step).
    const SimEvents& events() const { return events_; }

    // Debug fly mode: never dies; still lands on surfaces; still wins at the end wall.
    void setInvincible(bool on) { invincible_ = on; }
    bool invincible() const { return invincible_; }

    Snapshot snapshot() const { return Snapshot{s_}; }
    void restore(const Snapshot& snap) {
        s_ = snap.state;
        events_.clear();
    }

    const Level& level() const { return *level_; }

    // Boxes used by the sim (also handy for renderer debug overlays and tests).
    static Aabb outerBox(const PlayerState& p);
    static Aabb innerBox(const PlayerState& p);
    static Aabb spikeHitbox(const Object& spike);

private:
    template <class Fn>
    void forEachNear(double x0, double x1, Fn&& fn) const;
    void firePortals();
    void resolveVertical(double prevY, double vyBefore);
    bool touchesDeadly() const;

    const Level* level_;
    std::shared_ptr<const detail::SimIndex> index_;
    PlayerState s_;
    SimEvents events_;
    bool invincible_ = false;
};

}  // namespace gd
