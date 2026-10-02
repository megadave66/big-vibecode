#include "collision.hpp"

namespace flappy {

bool intersects(const Rect& a, const Rect& b) {
    if (a.w <= 0.0f || a.h <= 0.0f || b.w <= 0.0f || b.h <= 0.0f) {
        return false;
    }
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

}  // namespace flappy
