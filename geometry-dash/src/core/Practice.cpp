#include "core/Practice.h"

namespace gd {

bool Practice::add(const Sim& sim) {
    if (sim.dead() || sim.won()) return false;
    stack_.push_back(sim.snapshot());
    return true;
}

void Practice::removeLast() {
    if (!stack_.empty()) stack_.pop_back();
}

void Practice::respawn(Sim& sim) const {
    if (stack_.empty()) {
        sim.reset();
    } else {
        sim.restore(stack_.back());
    }
}

}  // namespace gd
