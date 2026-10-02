#include "core/Solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_set>

#include "core/PhysicsConstants.h"
#include "core/Sim.h"

namespace gd {

namespace {

using Clock = std::chrono::steady_clock;

// One search pass: decision grain and state buckets for the visited set.
struct Pass {
    int grain;          // ticks per decision
    double yq, vyq;     // y / vy bucket sizes
    long long maxNodes; // node budget (deterministic)
};

constexpr Pass kPasses[] = {
    {6, 0.10, 1.00, 2'000'000},
    {4, 0.05, 0.50, 4'000'000},
    {2, 0.02, 0.25, 8'000'000},
    {1, 0.01, 0.10, 16'000'000},
};

// Polish step budget (deterministic). About 10-20 s of sim on a laptop core.
constexpr long long kPolishStepBudget = 250'000'000;

std::uint64_t mix(std::uint64_t h, std::uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= h >> 31;
    h *= 0xbf58476d1ce4e5b9ULL;
    return h ^ (h >> 29);
}

std::uint64_t stateKey(const PlayerState& p, const Pass& pass) {
    std::uint64_t h = static_cast<std::uint64_t>(p.tick);
    h = mix(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(p.y / pass.yq))));
    h = mix(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(p.vy / pass.vyq))));
    const std::uint64_t flags = static_cast<std::uint64_t>(p.mode) | (static_cast<std::uint64_t>(p.gravity) << 2) |
                                (static_cast<std::uint64_t>(p.grounded) << 4) |
                                (static_cast<std::uint64_t>(p.prevHeld) << 5) |
                                (static_cast<std::uint64_t>((p.bufferTicks + 3) / 4) << 6) |
                                (static_cast<std::uint64_t>(p.speed) << 16);
    return mix(h, flags);
}

// Physics-relevant equality (rotation is visual only).
bool samePhys(const PlayerState& a, const PlayerState& b) {
    return a.tick == b.tick && a.y == b.y && a.vy == b.vy && a.x == b.x && a.vx == b.vx &&
           a.mode == b.mode && a.gravity == b.gravity && a.speed == b.speed &&
           a.grounded == b.grounded && a.dead == b.dead && a.won == b.won &&
           a.prevHeld == b.prevHeld && a.bufferTicks == b.bufferTicks && a.nextPortal == b.nextPortal;
}

struct SearchCtx {
    const Level& level;
    Sim sim;
    Clock::time_point deadline;
    SolverResult& res;
    bool aborted = false;

    void track() {
        const PlayerState& p = sim.player();
        if (p.x > res.furthestX) {
            res.furthestX = p.x;
            res.furthestTick = p.tick;
        }
    }
};

struct Frame {
    Snapshot snap;
    std::uint8_t tried = 0;  // choices tried so far (0..2)
    bool first = false;      // choice tried first
    bool chosen = false;     // most recent choice
};

// Depth-first search for one pass. Returns true and fills held/winTick on success.
bool runPass(SearchCtx& ctx, const Pass& pass, std::vector<bool>& held, int& winTick) {
    Sim& sim = ctx.sim;
    sim.reset();
    std::unordered_set<std::uint64_t> visited;
    visited.reserve(1 << 20);
    visited.insert(stateKey(sim.player(), pass));

    std::vector<Frame> stack;
    stack.push_back(Frame{sim.snapshot(), 0, false, false});
    long long passNodes = 0;

    while (!stack.empty()) {
        Frame& top = stack.back();
        if (top.tried == 2) {
            stack.pop_back();
            continue;
        }
        const bool choice = top.tried == 0 ? top.first : !top.first;
        ++top.tried;
        top.chosen = choice;

        if (++passNodes > pass.maxNodes) return false;
        ++ctx.res.nodes;
        if ((ctx.res.nodes & 1023) == 0 && Clock::now() > ctx.deadline) {
            ctx.aborted = true;
            return false;
        }

        sim.restore(top.snap);
        for (int k = 0; k < pass.grain && !sim.dead() && !sim.won(); ++k) {
            sim.step(choice);
            ++ctx.res.steps;
        }
        ctx.track();
        if (sim.dead()) continue;
        if (sim.won()) {
            winTick = sim.tick();
            held.assign(winTick, false);
            for (size_t i = 0; i < stack.size(); ++i) {
                const int t0 = static_cast<int>(i) * pass.grain;
                for (int t = t0; t < t0 + pass.grain && t < winTick; ++t) held[t] = stack[i].chosen;
            }
            return true;
        }
        if (!visited.insert(stateKey(sim.player(), pass)).second) continue;
        // Cube: prefer releasing (fewest presses). Ship: prefer keeping the current input.
        const bool first = sim.player().mode == GameMode::Ship ? choice : false;
        stack.push_back(Frame{sim.snapshot(), 0, first, false});
    }
    return false;
}

// ---------------------------------------------------------------- polish

struct Hold {
    int s, e;  // held on ticks [s, e)
};

std::vector<Hold> holdsOf(const std::vector<bool>& held) {
    std::vector<Hold> out;
    const int n = static_cast<int>(held.size());
    for (int t = 0; t < n;) {
        if (!held[t]) {
            ++t;
            continue;
        }
        int e = t;
        while (e < n && held[e]) ++e;
        out.push_back({t, e});
        t = e;
    }
    return out;
}

class Polisher {
public:
    Polisher(const Level& level, std::vector<bool> held, int winTick, SolverResult& res,
             Clock::time_point deadline)
        : sim_(level), held_(std::move(held)), W_(winTick), res_(res), deadline_(deadline) {
        base_.resize(W_ + 1);
        sim_.reset();
        rebuild(0);
    }

    bool outOfBudget() const { return budgetHit_; }
    const std::vector<bool>& held() const { return held_; }

    // Drop holds whose removal still wins (earliest first).
    void removePresses() {
        auto holds = holdsOf(held_);
        for (size_t i = 0; i < holds.size() && !checkBudget();) {
            std::vector<bool> h = held_;
            for (int t = holds[i].s; t < holds[i].e; ++t) h[t] = false;
            if (wins(h, holds[i].s, holds[i].e)) {
                commit(std::move(h), holds[i].s);
                holds.erase(holds.begin() + static_cast<long>(i));
            } else {
                ++i;
            }
        }
    }

    // Move each hold to the middle of its winning shift window.
    void centre(int cap) {
        auto holds = holdsOf(held_);
        for (size_t i = 0; i < holds.size() && !checkBudget(); ++i) {
            const int a = extent(holds, i, -1, cap);
            const int b = extent(holds, i, +1, cap);
            const int shift = (b - a) / 2;
            if (shift == 0) continue;
            std::vector<bool> h = shifted(holds[i], shift);
            commit(std::move(h), std::min(holds[i].s, holds[i].s + shift));
            holds[i].s += shift;
            holds[i].e += shift;
        }
    }

    // Smallest slack over all holds (capped). -1 when there are no holds.
    int measureSlack(int cap) {
        auto holds = holdsOf(held_);
        if (holds.empty()) return -1;
        int best = cap;
        for (size_t i = 0; i < holds.size() && !checkBudget(); ++i) {
            const int a = extent(holds, i, -1, best);
            if (a < best) best = a;
            const int b = extent(holds, i, +1, best);
            if (b < best) best = b;
            if (best == 0) break;
        }
        return best;
    }

private:
    bool checkBudget() {
        if (!budgetHit_ && (polishSteps_ > kPolishStepBudget || Clock::now() > deadline_)) budgetHit_ = true;
        return budgetHit_;
    }

    void step(bool h) {
        sim_.step(h);
        ++polishSteps_;
        ++res_.steps;
    }

    // Recompute baseline snapshots from tick `from` using held_.
    void rebuild(int from) {
        if (from == 0) {
            sim_.reset();
            base_[0] = sim_.snapshot();
        } else {
            sim_.restore(base_[from]);
        }
        for (int t = from; t < W_ && !sim_.dead() && !sim_.won(); ++t) {
            step(held_[t]);
            base_[t + 1] = sim_.snapshot();
        }
    }

    void commit(std::vector<bool> h, int from) {
        held_ = std::move(h);
        rebuild(std::max(0, from));
    }

    // True if script h wins. h equals held_ before `from` and from `sameFrom` on.
    bool wins(const std::vector<bool>& h, int from, int sameFrom) {
        from = std::max(0, from);
        sim_.restore(base_[from]);
        for (int t = from; t < W_; ++t) {
            if (t >= sameFrom && samePhys(sim_.player(), base_[t].state)) return true;
            step(h[t]);
            if (sim_.dead()) return false;
            if (sim_.won()) return true;
        }
        return sim_.won();
    }

    std::vector<bool> shifted(const Hold& hd, int d) const {
        std::vector<bool> h = held_;
        for (int t = hd.s; t < hd.e; ++t) h[t] = false;
        for (int t = hd.s + d; t < hd.e + d; ++t)
            if (t >= 0 && t < W_) h[t] = true;
        return h;
    }

    // Largest d in [0, cap] such that shifting hold i by dir*1 .. dir*d all win.
    int extent(const std::vector<Hold>& holds, size_t i, int dir, int cap) {
        const Hold& hd = holds[i];
        int d = 0;
        while (d < cap && !checkBudget()) {
            const int nd = (d + 1) * dir;
            // Keep at least one released tick between holds.
            if (dir < 0) {
                const int lo = i > 0 ? holds[i - 1].e + 1 : 0;
                if (hd.s + nd < lo) break;
            } else {
                const int hi = i + 1 < holds.size() ? holds[i + 1].s - 1 : W_;
                if (hd.e + nd > hi) break;
            }
            const std::vector<bool> h = shifted(hd, nd);
            if (!wins(h, std::min(hd.s, hd.s + nd), std::max(hd.e, hd.e + nd))) break;
            ++d;
        }
        return d;
    }

    Sim sim_;
    std::vector<bool> held_;
    int W_;
    SolverResult& res_;
    Clock::time_point deadline_;
    std::vector<Snapshot> base_;  // base_[t] = state before step t
    long long polishSteps_ = 0;
    bool budgetHit_ = false;
};

}  // namespace

SolverResult solveLevel(const Level& level, const SolverOptions& opts) {
    const auto start = Clock::now();
    const auto deadline =
        start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(opts.maxSeconds));
    SolverResult res;
    SearchCtx ctx{level, Sim(level), deadline, res};

    std::vector<bool> held;
    int winTick = 0;
    int passNo = 0;
    for (const Pass& pass : kPasses) {
        ++passNo;
        if (runPass(ctx, pass, held, winTick)) {
            res.solved = true;
            res.pass = passNo;
            break;
        }
        if (ctx.aborted) break;
    }
    res.timedOut = !res.solved && ctx.aborted;

    if (res.solved) {
        res.winTick = winTick;
        res.furthestX = std::max(res.furthestX, level.endX - phys::kPlayerSize);
        if (opts.polish || opts.measureSlack) {
            Polisher pol(level, held, winTick, res, deadline);
            if (opts.polish) {
                pol.removePresses();
                pol.centre(opts.slackCap);
            }
            if (opts.measureSlack) res.minSlack = pol.measureSlack(opts.slackCap);
            res.slackPartial = pol.outOfBudget();
            held = pol.held();
        }
        res.held = std::move(held);
        res.presses = static_cast<int>(holdsOf(res.held).size());
    }
    res.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    return res;
}

Replay solutionReplay(const Level& level, const SolverResult& result, int endMargin) {
    return replayFromHeld(level.id, result.held, result.winTick + std::max(0, endMargin));
}

}  // namespace gd
