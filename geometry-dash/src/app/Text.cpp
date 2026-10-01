#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gd {

TextRenderer::~TextRenderer() { shutdown(); }

bool TextRenderer::init(SDL_Renderer* renderer, const std::string& fontPath) {
    renderer_ = renderer;
    fontPath_ = fontPath;
    if (!TTF_Init()) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        renderer_ = nullptr;
        return false;
    }
    ttfInit_ = true;
    if (!font(24)) {
        renderer_ = nullptr;
        return false;
    }
    return true;
}

void TextRenderer::clearCache() {
    for (auto& kv : cache_)
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    cache_.clear();
}

void TextRenderer::shutdown() {
    clearCache();
    for (auto& kv : fonts_)
        if (kv.second) TTF_CloseFont(kv.second);
    fonts_.clear();
    if (ttfInit_) TTF_Quit();
    ttfInit_ = false;
    renderer_ = nullptr;
}

void TextRenderer::setPixelScale(float scale) {
    // Quantise to half steps so a window drag does not rebuild the cache every frame.
    float q = std::round(scale * 2.0f) / 2.0f;
    if (q < 1.0f) q = 1.0f;
    if (q == scale_) return;
    scale_ = q;
    clearCache();
    for (auto& kv : fonts_)
        if (kv.second) TTF_CloseFont(kv.second);
    fonts_.clear();
}

TTF_Font* TextRenderer::font(int size) {
    auto it = fonts_.find(size);
    if (it != fonts_.end()) return it->second;
    TTF_Font* f = TTF_OpenFont(fontPath_.c_str(), static_cast<float>(size) * scale_);
    if (!f) std::fprintf(stderr, "warning: TTF_OpenFont failed: %s\n", SDL_GetError());
    fonts_[size] = f;
    return f;
}

TextRenderer::Entry* TextRenderer::entry(const std::string& s, int size) {
    const std::string key = std::to_string(size) + ":" + s;
    auto it = cache_.find(key);
    if (it != cache_.end()) return &it->second;
    if (cache_.size() > 400) clearCache();
    Entry e;
    TTF_Font* f = font(size);
    if (f && !s.empty()) {
        SDL_Color white{255, 255, 255, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(f, s.c_str(), 0, white);
        if (surf) {
            e.tex = SDL_CreateTextureFromSurface(renderer_, surf);
            e.w = static_cast<float>(surf->w) / scale_;
            e.h = static_cast<float>(surf->h) / scale_;
            if (e.tex) SDL_SetTextureScaleMode(e.tex, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(surf);
        }
    }
    return &(cache_[key] = e);
}

void TextRenderer::measure(const std::string& s, int size, float& w, float& h) {
    w = h = 0;
    if (!ready()) return;
    Entry* e = entry(s, size);
    w = e->w;
    h = e->h;
}

void TextRenderer::draw(Gfx& gfx, const std::string& s, float x, float y, int size, Rgba color,
                        TextAlign align, bool shadow) {
    if (!ready() || s.empty()) return;
    gfx.flush();
    Entry* e = entry(s, size);
    if (!e->tex) return;
    float dx = x;
    if (align == TextAlign::Center) dx = x - e->w * 0.5f;
    else if (align == TextAlign::Right) dx = x - e->w;
    auto blit = [&](float ox, float oy, Rgba c) {
        SDL_SetTextureColorMod(e->tex, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(e->tex, c.a);
        const SDL_FRect dst{ox, oy, e->w, e->h};
        SDL_RenderTexture(renderer_, e->tex, nullptr, &dst);
    };
    if (shadow) {
        const float off = std::max(1.0f, size / 14.0f);
        blit(dx + off, y + off, rgba(0, 0, 0, color.a * 0.6));
    }
    blit(dx, y, color);
}

}  // namespace gd
