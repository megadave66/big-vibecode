// Axis-aligned bounding boxes. Pure math, no SDL.
#pragma once

namespace flappy {

// x,y = top-left corner, +y is down. w,h >= 0.
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

// True when the two boxes overlap with positive area.
// Boxes that only touch along an edge or corner do NOT intersect.
bool intersects(const Rect& a, const Rect& b);

}  // namespace flappy
