#include "GameRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/Interp.h"
#include "core/PhysicsConstants.h"

namespace gd {

namespace {

constexpr float kW = 1280.0f;
constexpr float kH = 720.0f;
constexpr double kPi = 3.14159265358979323846;

Rgba fromColor(Color c, int a = 255) { return rgba(c.r, c.g, c.b, a); }

struct Ctx {
    Gfx& g;
    const Camera& cam;
    const Level& lvl;
    double t;          // animation time, seconds
    float S;           // px per block
    Rgba bg, ground, accent;

    float sx(double wx) const { return static_cast<float>(cam.screenX(wx)); }
    float sy(double wy) const { return static_cast<float>(cam.screenY(wy)); }
};

std::uint32_t hash2(std::uint32_t a, std::uint32_t b) {
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

// ---------------------------------------------------------------- background
void drawParallaxLayer(Ctx& c, float factor, float cell, float minSize, float maxSize, int alpha,
                       std::uint32_t seed) {
    const float offX = static_cast<float>(c.cam.leftX()) * c.S * factor;
    const float offY = static_cast<float>(c.cam.centerY()) * c.S * factor * 0.35f;
    const int i0 = static_cast<int>(std::floor(offX / cell)) - 1;
    const int n = static_cast<int>(kW / cell) + 3;
    const Rgba tint = mix(c.bg, rgba(255, 255, 255), 0.45);
    const Rgba tint2 = mix(c.bg, c.accent, 0.55);
    for (int i = i0; i < i0 + n; ++i) {
        const std::uint32_t h = hash2(static_cast<std::uint32_t>(i + 100000), seed);
        const float fx = (h & 255) / 255.0f;
        const float fy = ((h >> 8) & 255) / 255.0f;
        const float fs = ((h >> 16) & 255) / 255.0f;
        const int shape = (h >> 24) & 3;
        const float size = minSize + (maxSize - minSize) * fs;
        const float x = i * cell - offX + fx * cell * 0.5f;
        const float y = kH * 0.08f + fy * kH * 0.62f + offY;
        const Rgba col = withAlpha((h & 0x100) ? tint : tint2, alpha);
        switch (shape) {
            case 0: c.g.outlineRect(x, y, size, size, 2.0f, col); break;
            case 1: c.g.fillRect(x, y, size, size, withAlpha(col, alpha / 2)); break;
            case 2: c.g.fillRect(x, y - size, 3.0f, size * 2.2f, col); break;
            default: {
                const Pt p[3] = {{x, y + size}, {x + size, y + size}, {x + size * 0.5f, y}};
                c.g.outlinePoly(p, 3, 2.0f, col);
                break;
            }
        }
    }
}

void drawBackground(Ctx& c) {
    c.g.gradientRect(0, 0, kW, kH, mix(c.bg, rgba(0, 0, 0), 0.5), mix(c.bg, rgba(255, 255, 255), 0.1));
    drawParallaxLayer(c, 0.10f, 260.0f, 80.0f, 170.0f, 36, 11u);
    drawParallaxLayer(c, 0.28f, 150.0f, 28.0f, 78.0f, 52, 29u);
}

// Ground band below y = 0 and ceiling band above y = ceiling.
void drawBand(Ctx& c, bool ceilingBand) {
    const float edgeY = ceilingBand ? c.sy(c.lvl.ceiling) : c.sy(0.0);
    float y0, y1;  // band spans [y0, y1] in screen y
    if (ceilingBand) {
        if (edgeY <= 0) return;
        y0 = 0; y1 = edgeY;
    } else {
        if (edgeY >= kH) return;
        y0 = edgeY; y1 = kH;
    }
    const Rgba top = ceilingBand ? mix(c.ground, rgba(0, 0, 0), 0.35) : c.ground;
    const Rgba bot = ceilingBand ? c.ground : mix(c.ground, rgba(0, 0, 0), 0.35);
    c.g.gradientRect(0, y0, kW, y1 - y0, top, bot);

    // Tile lines: vertical every block, horizontal every block, scrolling with the world.
    const Rgba line = withAlpha(mix(c.ground, c.accent, 0.35), 90);
    const int k0 = static_cast<int>(std::floor(c.cam.viewLeft())) - 1;
    const int k1 = static_cast<int>(std::ceil(c.cam.viewRight())) + 1;
    for (int k = k0; k <= k1; ++k) c.g.fillRect(c.sx(k) - 1.0f, y0, 2.0f, y1 - y0, line);
    for (int r = 1; r <= 14; ++r) {
        const float y = ceilingBand ? edgeY - r * c.S : edgeY + r * c.S;
        if (y < y0 - 2 || y > y1 + 2) continue;
        c.g.fillRect(0, y - 1.0f, kW, 2.0f, line);
    }
    // Tile corner studs.
    const Rgba stud = withAlpha(c.accent, 40);
    for (int k = k0; k <= k1; ++k) {
        const float y = ceilingBand ? edgeY - c.S * 0.5f : edgeY + c.S * 0.5f;
        c.g.fillRect(c.sx(k + 0.5) - 3.0f, y - 3.0f, 6.0f, 6.0f, stud);
    }
    // Accent edge line plus a soft glow on the playfield side.
    const float gh = 34.0f;
    c.g.setAdditive(true);
    if (ceilingBand) c.g.gradientRect(0, edgeY, kW, gh, withAlpha(c.accent, 80), withAlpha(c.accent, 0));
    else c.g.gradientRect(0, edgeY - gh, kW, gh, withAlpha(c.accent, 0), withAlpha(c.accent, 80));
    c.g.setAdditive(false);
    c.g.fillRect(0, edgeY - 2.5f, kW, 5.0f, mix(c.accent, rgba(255, 255, 255), 0.3));
}

// ---------------------------------------------------------------- objects
bool visible(const Ctx& c, const Object& o, double w, double h) {
    return o.x + w >= c.cam.viewLeft() - 2 && o.x <= c.cam.viewRight() + 2 && o.y + h >= c.cam.viewBottom() - 2 &&
           o.y <= c.cam.viewTop() + 2;
}

void drawBlock(Ctx& c, const Object& o) {
    const float x0 = c.sx(o.x), x1 = c.sx(o.x + o.w), yT = c.sy(o.y + o.h), yB = c.sy(o.y);
    const Rgba fill = mix(c.ground, rgba(0, 0, 0), 0.5);
    const Rgba edge = c.accent;
    c.g.fillRect(x0, yT, x1 - x0, yB - yT, fill);
    // Per-cell inner bevel and glow.
    for (double cy = 0; cy < o.h - 1e-6; cy += 1.0) {
        const double ch = std::min(1.0, o.h - cy);
        for (double cx = 0; cx < o.w - 1e-6; cx += 1.0) {
            const double cw = std::min(1.0, o.w - cx);
            const float a0 = c.sx(o.x + cx), a1 = c.sx(o.x + cx + cw);
            const float b0 = c.sy(o.y + cy + ch), b1 = c.sy(o.y + cy);
            if (a1 < -4 || a0 > kW + 4 || b1 < -4 || b0 > kH + 4) continue;
            const float in = 5.0f;
            c.g.fillRect(a0 + in, b0 + in, a1 - a0 - 2 * in, b1 - b0 - 2 * in, withAlpha(edge, 38));
            c.g.outlineRect(a0 + in, b0 + in, a1 - a0 - 2 * in, b1 - b0 - 2 * in, 1.5f, withAlpha(edge, 120));
            // Bevel: light top-left, dark bottom-right.
            c.g.fillRect(a0 + 1.5f, b0 + 1.5f, a1 - a0 - 3.0f, 2.0f, withAlpha(rgba(255, 255, 255), 60));
            c.g.fillRect(a0 + 1.5f, b1 - 3.5f, a1 - a0 - 3.0f, 2.0f, withAlpha(rgba(0, 0, 0), 90));
            c.g.outlineRect(a0, b0, a1 - a0, b1 - b0, 1.5f, withAlpha(edge, 150));
        }
    }
    c.g.outlineRect(x0, yT, x1 - x0, yB - yT, 3.0f, edge);
}

void drawPlatform(Ctx& c, const Object& o) {
    const float x0 = c.sx(o.x), x1 = c.sx(o.x + o.w), yT = c.sy(o.y + o.h), yB = c.sy(o.y);
    const Rgba fill = mix(c.ground, rgba(0, 0, 0), 0.4);
    c.g.gradientRect(x0, yT, x1 - x0, yB - yT, mix(fill, c.accent, 0.25), fill);
    for (double k = 0.5; k < o.w; k += 1.0) {
        const float px = c.sx(o.x + k);
        c.g.fillRect(px - 3, (yT + yB) * 0.5f - 3, 6, 6, withAlpha(c.accent, 160));
    }
    c.g.outlineRect(x0, yT, x1 - x0, yB - yT, 2.5f, c.accent);
    c.g.fillRect(x0, yT, x1 - x0, 4.0f, mix(c.accent, rgba(255, 255, 255), 0.5));
    c.g.fillRect(x0, yB - 3.0f, x1 - x0, 3.0f, withAlpha(rgba(0, 0, 0), 100));
}

void drawSpike(Ctx& c, const Object& o) {
    const bool up = o.spikeDir == SpikeDir::Up;
    const float xl = c.sx(o.x + 0.05), xr = c.sx(o.x + 0.95), xm = c.sx(o.x + 0.5);
    const float yBase = up ? c.sy(o.y) : c.sy(o.y + 1.0);
    const float yTip = up ? c.sy(o.y + 0.95) : c.sy(o.y + 0.05);
    const Pt tri[3] = {{xl, yBase}, {xr, yBase}, {xm, yTip}};
    const Rgba fill = mix(c.bg, rgba(0, 0, 0), 0.82);
    const Rgba edge = mix(c.accent, rgba(255, 255, 255), 0.65);
    // Soft glow.
    c.g.setAdditive(true);
    c.g.glowEllipse(xm, (yBase + yTip) * 0.5f, c.S * 0.75f, c.S * 0.7f, withAlpha(c.accent, 40));
    c.g.setAdditive(false);
    c.g.fillPoly(tri, 3, fill);
    // Inner triangle shine.
    const float k = 0.28f;
    const Pt inner[3] = {{xl + (xm - xl) * k, yBase + (yTip - yBase) * 0.12f},
                         {xr - (xr - xm) * k, yBase + (yTip - yBase) * 0.12f},
                         {xm, yBase + (yTip - yBase) * 0.62f}};
    c.g.fillPoly(inner, 3, withAlpha(c.accent, 55));
    c.g.outlinePoly(tri, 3, 3.0f, edge);
}

Rgba portalColor(const Object& o) {
    switch (o.type) {
        case ObjType::PortalGravity: return o.gravity == Gravity::Up ? rgba(255, 220, 50) : rgba(60, 140, 255);
        case ObjType::PortalMode: return o.mode == GameMode::Ship ? rgba(255, 100, 200) : rgba(80, 230, 100);
        case ObjType::PortalSpeed:
            switch (o.speed) {
                case Speed::Slow: return rgba(255, 170, 40);
                case Speed::Normal: return rgba(80, 200, 255);
                case Speed::Fast: return rgba(90, 235, 90);
                case Speed::Faster: return rgba(255, 90, 190);
            }
            break;
        default: break;
    }
    return rgba(255, 255, 255);
}

void drawPortal(Ctx& c, const Object& o) {
    const float cx = c.sx(o.x + o.w * 0.5), cy = c.sy(o.y + o.h * 0.5);
    const float rx = static_cast<float>(o.w * 0.42) * c.S, ry = static_cast<float>(o.h * 0.5) * c.S;
    const Rgba col = portalColor(o);
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(c.t * 5.0 + o.x));
    c.g.setAdditive(true);
    c.g.glowEllipse(cx, cy, rx * 2.4f, ry * 1.15f, withAlpha(col, 70 + static_cast<int>(40 * pulse)));
    c.g.setAdditive(false);
    c.g.fillEllipse(cx, cy, rx, ry, withAlpha(mix(col, rgba(0, 0, 0), 0.6), 150));
    c.g.ringEllipse(cx, cy, rx, ry, 6.0f, col);
    c.g.ringEllipse(cx, cy, rx * 0.72f, ry * 0.8f, 2.5f, withAlpha(mix(col, rgba(255, 255, 255), 0.6), 200));
    // Light dots running up the ring.
    c.g.setAdditive(true);
    for (int i = 0; i < 4; ++i) {
        const double a = c.t * 3.0 + i * kPi * 0.5 + o.x;
        const float dx = std::cos(static_cast<float>(a)) * rx, dy = std::sin(static_cast<float>(a)) * ry;
        c.g.glowEllipse(cx + dx, cy + dy, 12.0f, 12.0f, withAlpha(mix(col, rgba(255, 255, 255), 0.5), 180), 12);
    }
    c.g.setAdditive(false);

    // Icon in the middle.
    const Rgba ic = rgba(255, 255, 255, 235);
    const float u = c.S * 0.22f;
    if (o.type == ObjType::PortalGravity) {
        const float d = o.gravity == Gravity::Up ? -1.0f : 1.0f;  // screen y direction of the arrow
        // Arrow pointing in the gravity direction.
        const Pt head[3] = {{cx - u * 1.2f, cy + d * u * 0.2f}, {cx + u * 1.2f, cy + d * u * 0.2f}, {cx, cy + d * u * 1.6f}};
        c.g.fillPoly(head, 3, ic);
        c.g.fillRect(cx - u * 0.35f, std::min(cy - d * u * 1.5f, cy + d * u * 0.2f), u * 0.7f, u * 1.7f, ic);
    } else if (o.type == ObjType::PortalMode) {
        if (o.mode == GameMode::Cube) {
            c.g.outlineRect(cx - u * 1.2f, cy - u * 1.2f, u * 2.4f, u * 2.4f, 3.0f, ic);
            c.g.fillRect(cx - u * 0.45f, cy - u * 0.45f, u * 0.9f, u * 0.9f, withAlpha(ic, 160));
        } else {
            const Pt ship[4] = {{cx - u * 1.5f, cy - u * 0.9f}, {cx + u * 1.6f, cy}, {cx - u * 1.5f, cy + u * 0.9f},
                                {cx - u * 0.8f, cy}};
            c.g.fillPoly(ship, 4, ic);
        }
    } else if (o.type == ObjType::PortalSpeed) {
        const int n = static_cast<int>(o.speed) + 1;
        const float sp = u * 0.95f;
        const float x0 = cx - (n - 1) * sp * 0.5f;
        for (int i = 0; i < n; ++i) {
            const float x = x0 + i * sp;
            c.g.line(x - u * 0.5f, cy - u * 1.1f, x + u * 0.35f, cy, 3.5f, ic);
            c.g.line(x + u * 0.35f, cy, x - u * 0.5f, cy + u * 1.1f, 3.5f, ic);
        }
    }
}

void drawDeco(Ctx& c, const Object& o) {
    const float x0 = c.sx(o.x), x1 = c.sx(o.x + o.w), yT = c.sy(o.y + o.h), yB = c.sy(o.y);
    const float w = x1 - x0, h = yB - yT;
    const float cx = (x0 + x1) * 0.5f, cy = (yT + yB) * 0.5f;
    const Rgba soft = withAlpha(mix(c.bg, c.accent, 0.6), 70);
    switch (o.deco) {
        case DecoKind::Pillar:
            c.g.fillRect(x0 + w * 0.2f, yT, w * 0.6f, h, withAlpha(mix(c.bg, rgba(0, 0, 0), 0.4), 150));
            c.g.fillRect(x0 + w * 0.2f, yT, 3.0f, h, soft);
            c.g.fillRect(x0 + w * 0.8f - 3.0f, yT, 3.0f, h, soft);
            c.g.fillRect(x0 + w * 0.1f, yT, w * 0.8f, 8.0f, soft);
            c.g.fillRect(x0 + w * 0.1f, yB - 8.0f, w * 0.8f, 8.0f, soft);
            break;
        case DecoKind::Chain: {
            const int links = std::max(1, static_cast<int>(o.h / 0.45));
            for (int i = 0; i < links; ++i) {
                const float y = yB - (i + 0.5f) * h / links;
                if (i % 2 == 0) c.g.ringEllipse(cx, y, w * 0.18f + 3.0f, h / links * 0.62f, 3.0f, withAlpha(c.accent, 150), 14);
                else c.g.ringEllipse(cx, y, 4.0f, h / links * 0.62f, 3.0f, withAlpha(c.accent, 150), 14);
            }
            break;
        }
        case DecoKind::Star: {
            const float r = std::min(w, h) * 0.5f;
            Pt p[8];
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i * kPi / 4.0 + c.t * 0.3);
                const float rr = (i % 2 == 0) ? r : r * 0.28f;
                p[i] = {cx + std::cos(a) * rr, cy + std::sin(a) * rr};
            }
            c.g.setAdditive(true);
            c.g.glowEllipse(cx, cy, r * 1.6f, r * 1.6f, withAlpha(c.accent, 70));
            c.g.setAdditive(false);
            c.g.fillPoly(p, 8, withAlpha(mix(c.accent, rgba(255, 255, 255), 0.5), 200));
            break;
        }
        case DecoKind::Arrow: {
            const Rgba col = withAlpha(c.accent, 150);
            c.g.line(cx - w * 0.3f, cy - h * 0.4f, cx + w * 0.3f, cy, 5.0f, col);
            c.g.line(cx + w * 0.3f, cy, cx - w * 0.3f, cy + h * 0.4f, 5.0f, col);
            break;
        }
        case DecoKind::Glow:
            c.g.setAdditive(true);
            c.g.glowEllipse(cx, cy, w * 0.75f, h * 0.75f, withAlpha(c.accent, 110));
            c.g.setAdditive(false);
            break;
    }
}

void drawEndWall(Ctx& c, const Object& o) {
    const float x = c.sx(o.x);
    if (x > kW + 200) return;
    const float bw = c.S * 0.45f;
    // Dark finish zone beyond the wall.
    c.g.fillRect(x + bw, 0, kW, kH, withAlpha(rgba(0, 0, 0), 150));
    // Glow strips to the left of the wall.
    c.g.setAdditive(true);
    const float pulse = 0.8f + 0.2f * static_cast<float>(std::sin(c.t * 4.0));
    for (int i = 0; i < 8; ++i) {
        const float w = c.S * 0.28f;
        c.g.fillRect(x - (i + 1) * w, 0, w, kH, withAlpha(c.accent, static_cast<int>((70 - i * 8) * pulse)));
    }
    c.g.setAdditive(false);
    c.g.fillRect(x, 0, bw, kH, mix(c.accent, rgba(255, 255, 255), 0.75));
    c.g.fillRect(x + bw * 0.3f, 0, bw * 0.4f, kH, rgba(255, 255, 255));
    // Light beams
    c.g.setAdditive(true);
    for (int i = 0; i < 5; ++i) {
        const float by = c.sy(c.lvl.ceiling * (0.1 + 0.2 * i)) + 10.0f * static_cast<float>(std::sin(c.t * 3 + i));
        c.g.glowEllipse(x + bw * 0.5f, by, c.S * 1.1f, c.S * 0.9f, withAlpha(rgba(255, 255, 255), 60));
    }
    c.g.setAdditive(false);
}

// ---------------------------------------------------------------- player
struct LocalXf {
    const Ctx& c;
    double cx, cy;       // world centre
    double deg;          // CCW rotation (y up)
    double flip;         // +1 normal, -1 mirror in local y
    double dy = 0.0;     // local y shift applied before mirroring
    Pt at(double lx, double ly) const {
        ly = (ly + dy) * flip;
        const double a = deg * kPi / 180.0;
        const double wx = cx + lx * std::cos(a) - ly * std::sin(a);
        const double wy = cy + lx * std::sin(a) + ly * std::cos(a);
        return Pt{c.sx(wx), c.sy(wy)};
    }
};

void polyLocal(Ctx& c, const LocalXf& xf, std::initializer_list<std::pair<double, double>> pts, Rgba fill,
               float outline, Rgba edge) {
    std::vector<Pt> p;
    for (const auto& q : pts) p.push_back(xf.at(q.first, q.second));
    if (xf.flip < 0) std::reverse(p.begin(), p.end());  // mirrored winding does not matter for fills, keep tidy
    if (fill.a) c.g.fillPoly(p, fill);
    if (outline > 0) c.g.outlinePoly(p, outline, edge);
}

void drawPlayer(Ctx& c, const PlayerPose& p, const Level&) {
    const double cx = p.x + 0.5, cy = p.y + 0.5;
    const Rgba dark = rgba(18, 18, 48);
    c.g.setAdditive(true);
    c.g.glowEllipse(c.sx(cx), c.sy(cy), c.S * 1.3f, c.S * 1.3f, withAlpha(rgba(255, 220, 90), 55));
    c.g.setAdditive(false);

    if (p.mode == GameMode::Cube) {
        LocalXf xf{c, cx, cy, p.rotation, 1.0};
        polyLocal(c, xf, {{-0.5, -0.5}, {0.5, -0.5}, {0.5, 0.5}, {-0.5, 0.5}}, rgba(255, 205, 50), 3.5f, dark);
        polyLocal(c, xf, {{-0.36, -0.36}, {0.36, -0.36}, {0.36, 0.36}, {-0.36, 0.36}}, rgba(255, 238, 140), 0, dark);
        // Face: two eyes and a mouth bar.
        polyLocal(c, xf, {{-0.24, 0.02}, {-0.08, 0.02}, {-0.08, 0.2}, {-0.24, 0.2}}, dark, 0, dark);
        polyLocal(c, xf, {{0.08, 0.02}, {0.24, 0.02}, {0.24, 0.2}, {0.08, 0.2}}, dark, 0, dark);
        polyLocal(c, xf, {{-0.22, -0.2}, {0.22, -0.2}, {0.22, -0.1}, {-0.22, -0.1}}, dark, 0, dark);
    } else {
        // Ship: hull, tail fin, cockpit cube, flame. Mirrored when gravity points up.
        LocalXf xf{c, cx, cy, p.rotation, p.gravity == Gravity::Up ? -1.0 : 1.0, -0.12};
        const float flick = static_cast<float>(0.5 + 0.5 * std::sin(c.t * 45.0));
        c.g.setAdditive(true);
        {
            const double len = 0.28 + 0.2 * flick;
            const Pt f[3] = {xf.at(-0.54, 0.0), xf.at(-0.54, -0.16), xf.at(-0.54 - len, -0.08)};
            c.g.fillPoly(f, 3, rgba(255, 170, 40, 230));
            const Pt f2[3] = {xf.at(-0.54, -0.02), xf.at(-0.54, -0.14), xf.at(-0.54 - len * 0.55, -0.08)};
            c.g.fillPoly(f2, 3, rgba(255, 245, 160, 230));
        }
        c.g.setAdditive(false);
        polyLocal(c, xf, {{-0.56, 0.12}, {-0.72, 0.42}, {-0.3, 0.22}}, rgba(235, 70, 170), 2.5f, dark);  // fin
        polyLocal(c, xf, {{-0.56, -0.22}, {0.45, -0.22}, {0.62, -0.02}, {0.26, 0.22}, {-0.2, 0.26}, {-0.56, 0.12}},
                  rgba(255, 105, 200), 3.5f, dark);
        polyLocal(c, xf, {{-0.52, -0.18}, {0.42, -0.18}, {0.5, -0.08}, {-0.52, -0.06}}, rgba(190, 50, 140), 0, dark);
        polyLocal(c, xf, {{-0.22, 0.2}, {0.2, 0.2}, {0.2, 0.58}, {-0.22, 0.58}}, rgba(255, 215, 70), 3.0f, dark);
        polyLocal(c, xf, {{-0.12, 0.34}, {-0.03, 0.34}, {-0.03, 0.46}, {-0.12, 0.46}}, dark, 0, dark);
        polyLocal(c, xf, {{0.04, 0.34}, {0.13, 0.34}, {0.13, 0.46}, {0.04, 0.46}}, dark, 0, dark);
    }
}

void drawParticles(Ctx& c, const ParticleSystem& ps) {
    for (int pass = 0; pass < 2; ++pass) {
        c.g.setAdditive(pass == 1);
        for (const Particle& p : ps.particles()) {
            if (p.additive != (pass == 1)) continue;
            const int a = p.alphaNow();
            if (a <= 0) continue;
            const Rgba col = fromColor(p.color, a);
            const float cx = c.sx(p.x), cy = c.sy(p.y);
            const float s = static_cast<float>(p.size()) * c.S;
            if (p.shape == ParticleShape::Ring) {
                c.g.ringEllipse(cx, cy, s * 0.5f, s * 0.5f, 2.0f + 5.0f * static_cast<float>(1.0 - p.t()), col, 40);
            } else {
                const float h = s * 0.5f;
                Pt q[4] = {{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}};
                for (Pt& pt : q) pt = rotatePt(pt, {cx, cy}, -p.rot);
                c.g.fillPoly(q, 4, col);
            }
        }
    }
    c.g.setAdditive(false);
}

void drawCheckpoints(Ctx& c, const GameScreen& game) {
    for (const Snapshot& s : game.practice().checkpoints()) {
        const float cx = c.sx(s.state.x + 0.5), cy = c.sy(s.state.y + 0.5);
        const float r = c.S * 0.42f;
        const Pt d[4] = {{cx, cy - r}, {cx + r * 0.72f, cy}, {cx, cy + r}, {cx - r * 0.72f, cy}};
        c.g.setAdditive(true);
        c.g.glowEllipse(cx, cy, r * 2.0f, r * 2.0f, rgba(60, 255, 120, 70));
        c.g.setAdditive(false);
        c.g.fillPoly(d, 4, rgba(50, 220, 100));
        c.g.outlinePoly(d, 4, 3.0f, rgba(220, 255, 230));
    }
}

void drawHitboxes(Ctx& c, const GameScreen& game, const PlayerPose&) {
    const Sim& sim = game.sim();
    auto box = [&](const Aabb& b, Rgba col) {
        c.g.outlineRect(c.sx(b.x), c.sy(b.top()), static_cast<float>(b.w) * c.S, static_cast<float>(b.h) * c.S, 2.0f, col);
    };
    box(Sim::outerBox(sim.player()), rgba(60, 255, 90));
    box(Sim::innerBox(sim.player()), rgba(255, 60, 60));
    for (const Object& o : game.level().objects) {
        if (o.x > c.cam.viewRight() + 2) break;
        if (o.type == ObjType::Spike) box(Sim::spikeHitbox(o), rgba(255, 60, 255));
        else if (o.type == ObjType::Block) box(Aabb{o.x, o.y, o.w, o.h}, rgba(255, 220, 40));
        else if (o.type == ObjType::Platform) box(Aabb{o.x, o.y, o.w, phys::kPlatformHeight}, rgba(255, 220, 40));
    }
}

void drawHud(Ctx& c, TextRenderer& text, const GameScreen& game) {
    // Progress bar.
    const float bw = 460.0f, bh = 14.0f, bx = (kW - bw) * 0.5f, by = 22.0f;
    c.g.fillRect(bx - 2, by - 2, bw + 4, bh + 4, rgba(0, 0, 0, 150));
    c.g.fillRect(bx, by, bw, bh, rgba(255, 255, 255, 40));
    const float fill = bw * static_cast<float>(clampd(game.progress(), 0.0, 1.0));
    c.g.gradientRect(bx, by, fill, bh, mix(c.accent, rgba(255, 255, 255), 0.6), c.accent);
    {
        // The font's % glyph is hard to read, so draw the sign by hand.
        const std::string num = std::to_string(game.percent());
        float tw = 0, th = 0;
        text.measure(num, 28, tw, th);
        const float tx = bx + bw + 14;
        text.draw(c.g, num, tx, by - 9, 28, rgba(255, 255, 255));
        const float px = tx + tw + 8, py = by + 7;
        const Rgba w = rgba(255, 255, 255);
        c.g.ringEllipse(px - 7, py - 6, 4.5f, 4.5f, 2.5f, w, 14);
        c.g.ringEllipse(px + 7, py + 6, 4.5f, 4.5f, 2.5f, w, 14);
        c.g.line(px + 8, py - 11, px - 8, py + 11, 2.5f, w);
    }
    if (game.practiceMode()) {
        text.draw(c.g, "PRACTICE", 20, 14, 26, rgba(120, 255, 160));
        text.draw(c.g, "Checkpoints " + std::to_string(game.practice().count()), 20, 46, 20, rgba(220, 255, 230));
        text.draw(c.g, "Z / C place   X remove", 20, 72, 16, rgba(220, 235, 255, 200));
    }
    if (game.fly()) text.draw(c.g, "FLY", kW - 20, 14, 30, rgba(255, 120, 120), TextAlign::Right);
}

}  // namespace

void drawGame(Gfx& gfx, TextRenderer& text, const GameScreen& game, double alpha, const DrawOptions& opt) {
    const Level& lvl = game.level();
    const Camera& cam = game.camera();
    Ctx c{gfx, cam, lvl, game.animTime(), static_cast<float>(cam.config().pxPerBlock), fromColor(lvl.bg),
          fromColor(lvl.ground), fromColor(lvl.accent)};

    drawBackground(c);

    // Decoration behind everything else.
    for (const Object& o : lvl.objects) {
        if (o.x > cam.viewRight() + 3) break;
        if (o.type == ObjType::Deco && visible(c, o, o.w, o.h)) drawDeco(c, o);
    }
    drawBand(c, false);
    drawBand(c, true);

    for (const Object& o : lvl.objects) {
        if (o.x > cam.viewRight() + 3) break;
        switch (o.type) {
            case ObjType::EndWall: drawEndWall(c, o); break;
            case ObjType::Block: if (visible(c, o, o.w, o.h)) drawBlock(c, o); break;
            case ObjType::Platform: if (visible(c, o, o.w, o.h)) drawPlatform(c, o); break;
            default: break;
        }
    }
    for (const Object& o : lvl.objects) {
        if (o.x > cam.viewRight() + 3) break;
        if (o.type == ObjType::Spike && visible(c, o, 1, 1)) drawSpike(c, o);
    }
    for (const Object& o : lvl.objects) {
        if (o.x > cam.viewRight() + 3) break;
        if ((o.type == ObjType::PortalGravity || o.type == ObjType::PortalMode || o.type == ObjType::PortalSpeed) &&
            visible(c, o, o.w, o.h))
            drawPortal(c, o);
    }

    drawCheckpoints(c, game);

    // "Attempt N" in the world, near the start of the level.
    if (!game.practiceMode() || game.practice().count() == 0) {
        gfx.flush();
        const float x = c.sx(8.0), y = c.sy(6.5);
        if (x > -400 && x < kW + 400)
            text.draw(gfx, "Attempt " + std::to_string(game.attempts()), x, y, 46, rgba(255, 255, 255, 235),
                      TextAlign::Center);
    }

    const bool showPlayer = !game.dying() && !(game.sim().dead());
    const PlayerPose pose = game.pose(alpha);
    drawParticles(c, game.particles());
    if (showPlayer) drawPlayer(c, pose, lvl);
    if (opt.hitboxes) drawHitboxes(c, game, pose);

    // Death flash.
    if (game.dying()) {
        const int since = GameScreen::kDeathTicks - game.deathTicksLeft();
        if (since < 24) gfx.fillRect(0, 0, kW, kH, rgba(255, 255, 255, 90 - since * 90 / 24));
    }
    if (opt.hud) drawHud(c, text, game);
    gfx.flush();
}

}  // namespace gd
