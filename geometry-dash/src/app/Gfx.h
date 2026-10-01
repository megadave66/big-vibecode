#pragma once
// Batched 2D drawing helpers on top of SDL_RenderGeometry. All coordinates are logical pixels
// (the renderer uses logical presentation). Primitives are queued and drawn by flush(); call
// flush() before drawing textures (text) so the draw order stays right.

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace gd {

struct Rgba {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};

inline Rgba rgba(int r, int g, int b, int a = 255) {
    auto c = [](int v) { return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    return Rgba{c(r), c(g), c(b), c(a)};
}
inline Rgba withAlpha(Rgba c, int a) { return rgba(c.r, c.g, c.b, a); }
// Linear mix: t = 0 gives a, t = 1 gives b (alpha mixed too).
Rgba mix(Rgba a, Rgba b, double t);
Rgba scaleRgb(Rgba c, double f);   // multiply r,g,b (clamped), keep alpha

struct Pt {
    float x = 0, y = 0;
};

class Gfx {
public:
    explicit Gfx(SDL_Renderer* r) : r_(r) {}

    SDL_Renderer* renderer() const { return r_; }

    void flush();
    // Additive blending for glows. Flushes on change.
    void setAdditive(bool on);

    void clear(Rgba c);
    void fillRect(float x, float y, float w, float h, Rgba c);
    // Vertical gradient.
    void gradientRect(float x, float y, float w, float h, Rgba top, Rgba bottom);
    void fillTriangle(Pt a, Pt b, Pt c, Rgba col);
    // Convex polygon (triangle fan).
    void fillPoly(const Pt* pts, int n, Rgba col);
    void fillPoly(const std::vector<Pt>& pts, Rgba col) { fillPoly(pts.data(), static_cast<int>(pts.size()), col); }
    void line(float x0, float y0, float x1, float y1, float thickness, Rgba c);
    // Closed outline, thickness in px, drawn centred on the polygon edge.
    void outlinePoly(const Pt* pts, int n, float thickness, Rgba c);
    void outlinePoly(const std::vector<Pt>& pts, float thickness, Rgba c) {
        outlinePoly(pts.data(), static_cast<int>(pts.size()), thickness, c);
    }
    void outlineRect(float x, float y, float w, float h, float thickness, Rgba c);  // drawn inside the rect
    void fillEllipse(float cx, float cy, float rx, float ry, Rgba c, int segments = 32);
    // Radial fill: centre colour fades to transparent at the edge (soft glow).
    void glowEllipse(float cx, float cy, float rx, float ry, Rgba centre, int segments = 28);
    void ringEllipse(float cx, float cy, float rx, float ry, float thickness, Rgba c, int segments = 36);

private:
    void push(const Pt& p, Rgba c);
    void quadIdx(int a, int b, int c, int d);
    int base() const { return static_cast<int>(verts_.size()); }
    SDL_Renderer* r_;
    std::vector<SDL_Vertex> verts_;
    std::vector<int> idx_;
    bool additive_ = false;
};

// Rotated rectangle / polygon helpers: rotate points around (cx, cy) by `deg` degrees,
// counter-clockwise as seen on screen with y DOWN being flipped by the caller.
Pt rotatePt(Pt p, Pt centre, double degScreenCw);

}  // namespace gd
