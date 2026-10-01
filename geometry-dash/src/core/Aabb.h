#pragma once
// Axis-aligned box. x,y = bottom-left corner, y grows up. Units: blocks.

#include <algorithm>

namespace gd {

struct Aabb {
    double x = 0, y = 0, w = 0, h = 0;

    double right() const { return x + w; }
    double top() const { return y + h; }
    double centerX() const { return x + w * 0.5; }
    double centerY() const { return y + h * 0.5; }
};

// Strict overlap: boxes that only touch along an edge or corner do NOT overlap.
inline bool overlaps(const Aabb& a, const Aabb& b) {
    return a.x < b.right() && b.x < a.right() && a.y < b.top() && b.y < a.top();
}

// Overlapping region. Returns a zero-size box at the clamped position when there is no overlap.
inline Aabb intersection(const Aabb& a, const Aabb& b) {
    const double x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const double x1 = std::min(a.right(), b.right()), y1 = std::min(a.top(), b.top());
    if (x1 <= x0 || y1 <= y0) return Aabb{x0, y0, 0.0, 0.0};
    return Aabb{x0, y0, x1 - x0, y1 - y0};
}

// Point inside box. Left/bottom edges are inclusive, right/top edges exclusive.
inline bool contains(const Aabb& box, double px, double py) {
    return px >= box.x && px < box.right() && py >= box.y && py < box.top();
}

// Box fully inside outer (edges may touch).
inline bool contains(const Aabb& outer, const Aabb& inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.right() <= outer.right() &&
           inner.top() <= outer.top();
}

inline Aabb translate(const Aabb& box, double dx, double dy) {
    return Aabb{box.x + dx, box.y + dy, box.w, box.h};
}

}  // namespace gd
