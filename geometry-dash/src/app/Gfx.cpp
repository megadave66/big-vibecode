#include "Gfx.h"

#include <algorithm>
#include <cmath>

namespace gd {

Rgba mix(Rgba a, Rgba b, double t) {
    auto m = [t](int x, int y) { return static_cast<int>(x + (y - x) * t + 0.5); };
    return rgba(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
}

Rgba scaleRgb(Rgba c, double f) {
    return rgba(static_cast<int>(c.r * f), static_cast<int>(c.g * f), static_cast<int>(c.b * f), c.a);
}

Pt rotatePt(Pt p, Pt c, double deg) {
    const double a = deg * 3.14159265358979323846 / 180.0;
    const double s = std::sin(a), co = std::cos(a);
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Pt{static_cast<float>(c.x + dx * co - dy * s), static_cast<float>(c.y + dx * s + dy * co)};
}

void Gfx::push(const Pt& p, Rgba c) {
    SDL_Vertex v;
    v.position.x = p.x;
    v.position.y = p.y;
    v.color = SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    v.tex_coord = SDL_FPoint{0, 0};
    verts_.push_back(v);
}

void Gfx::quadIdx(int a, int b, int c, int d) {
    idx_.insert(idx_.end(), {a, b, c, a, c, d});
}

void Gfx::flush() {
    if (!verts_.empty()) {
        SDL_RenderGeometry(r_, nullptr, verts_.data(), static_cast<int>(verts_.size()), idx_.data(),
                           static_cast<int>(idx_.size()));
    }
    verts_.clear();
    idx_.clear();
}

void Gfx::setAdditive(bool on) {
    if (on == additive_) return;
    flush();
    additive_ = on;
    SDL_SetRenderDrawBlendMode(r_, on ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
}

void Gfx::clear(Rgba c) {
    flush();
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
    SDL_RenderClear(r_);
}

void Gfx::fillRect(float x, float y, float w, float h, Rgba c) { gradientRect(x, y, w, h, c, c); }

void Gfx::gradientRect(float x, float y, float w, float h, Rgba top, Rgba bottom) {
    const int b = base();
    push({x, y}, top);
    push({x + w, y}, top);
    push({x + w, y + h}, bottom);
    push({x, y + h}, bottom);
    quadIdx(b, b + 1, b + 2, b + 3);
}

void Gfx::fillTriangle(Pt a, Pt b, Pt c, Rgba col) {
    const int i = base();
    push(a, col);
    push(b, col);
    push(c, col);
    idx_.insert(idx_.end(), {i, i + 1, i + 2});
}

void Gfx::fillPoly(const Pt* pts, int n, Rgba col) {
    if (n < 3) return;
    const int i = base();
    for (int k = 0; k < n; ++k) push(pts[k], col);
    for (int k = 1; k + 1 < n; ++k) idx_.insert(idx_.end(), {i, i + k, i + k + 1});
}

void Gfx::line(float x0, float y0, float x1, float y1, float thickness, Rgba c) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    const float nx = -dy / len * thickness * 0.5f, ny = dx / len * thickness * 0.5f;
    const int b = base();
    push({x0 + nx, y0 + ny}, c);
    push({x1 + nx, y1 + ny}, c);
    push({x1 - nx, y1 - ny}, c);
    push({x0 - nx, y0 - ny}, c);
    quadIdx(b, b + 1, b + 2, b + 3);
}

void Gfx::outlinePoly(const Pt* pts, int n, float thickness, Rgba c) {
    for (int k = 0; k < n; ++k) {
        const Pt& a = pts[k];
        const Pt& b = pts[(k + 1) % n];
        line(a.x, a.y, b.x, b.y, thickness, c);
        // Small square at the joint hides gaps between segments.
        const float h = thickness * 0.5f;
        fillRect(b.x - h, b.y - h, thickness, thickness, c);
    }
}

void Gfx::outlineRect(float x, float y, float w, float h, float t, Rgba c) {
    fillRect(x, y, w, t, c);
    fillRect(x, y + h - t, w, t, c);
    fillRect(x, y + t, t, h - 2 * t, c);
    fillRect(x + w - t, y + t, t, h - 2 * t, c);
}

void Gfx::fillEllipse(float cx, float cy, float rx, float ry, Rgba c, int seg) {
    const int b = base();
    push({cx, cy}, c);
    for (int k = 0; k < seg; ++k) {
        const double a = 2.0 * 3.14159265358979323846 * k / seg;
        push({cx + static_cast<float>(std::cos(a)) * rx, cy + static_cast<float>(std::sin(a)) * ry}, c);
    }
    for (int k = 0; k < seg; ++k) idx_.insert(idx_.end(), {b, b + 1 + k, b + 1 + (k + 1) % seg});
}

void Gfx::glowEllipse(float cx, float cy, float rx, float ry, Rgba centre, int seg) {
    const int b = base();
    push({cx, cy}, centre);
    Rgba edge = centre;
    edge.a = 0;
    for (int k = 0; k < seg; ++k) {
        const double a = 2.0 * 3.14159265358979323846 * k / seg;
        push({cx + static_cast<float>(std::cos(a)) * rx, cy + static_cast<float>(std::sin(a)) * ry}, edge);
    }
    for (int k = 0; k < seg; ++k) idx_.insert(idx_.end(), {b, b + 1 + k, b + 1 + (k + 1) % seg});
}

void Gfx::ringEllipse(float cx, float cy, float rx, float ry, float t, Rgba c, int seg) {
    const int b = base();
    const float irx = std::max(0.0f, rx - t), iry = std::max(0.0f, ry - t);
    for (int k = 0; k < seg; ++k) {
        const double a = 2.0 * 3.14159265358979323846 * k / seg;
        const float co = static_cast<float>(std::cos(a)), si = static_cast<float>(std::sin(a));
        push({cx + co * rx, cy + si * ry}, c);
        push({cx + co * irx, cy + si * iry}, c);
    }
    for (int k = 0; k < seg; ++k) {
        const int o0 = b + 2 * k, i0 = o0 + 1;
        const int o1 = b + 2 * ((k + 1) % seg), i1 = o1 + 1;
        idx_.insert(idx_.end(), {o0, i0, i1, o0, i1, o1});
    }
}

}  // namespace gd
