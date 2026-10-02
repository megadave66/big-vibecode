#pragma once
// Practice mode: a stack of checkpoints (sim snapshots). Normal mode restarts with Sim::reset().
// Owner: physics section.

#include <vector>

#include "core/Sim.h"

namespace gd {

class Practice {
public:
    // Push a checkpoint at the sim's current state. Only while alive (and not won).
    bool add(const Sim& sim);
    // Drop the most recent checkpoint (no-op if none).
    void removeLast();
    // Restore the last checkpoint, or reset the sim if there is none.
    void respawn(Sim& sim) const;
    int count() const { return static_cast<int>(stack_.size()); }
    void clear() { stack_.clear(); }
    const std::vector<Snapshot>& checkpoints() const { return stack_; }

private:
    std::vector<Snapshot> stack_;
};

}  // namespace gd
