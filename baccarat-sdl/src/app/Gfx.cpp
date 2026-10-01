#include "Gfx.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bac::app::gfx {

TextCache::~TextCache() {
  for (auto& kv : map_)
    if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
}

const TextCache::Entry* TextCache::get(TTF_Font* font, const std::string& text, SDL_Color c) {
  std::string key = std::to_string(reinterpret_cast<std::uintptr_t>(font)) + '|' +
                    std::to_string((c.r << 24) | (c.g << 16) | (c.b << 8) | c.a) + '|' + text;
  auto it = map_.find(key);
  if (it != map_.end()) return &it->second;
  if (map_.size() > 600) {  // bounded: drop everything, it refills on demand
    for (auto& kv : map_)
      if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    map_.clear();
  }
  Entry e;
  if (!text.empty()) {
    if (SDL_Surface* s = TTF_RenderText_Blended(font, text.c_str(), 0, c)) {
      e.tex = SDL_CreateTextureFromSurface(r_, s);
      e.w = float(s->w);
      e.h = float(s->h);
      SDL_DestroySurface(s);
    }
  }
  return &map_.emplace(std::move(key), e).first->second;
}

void TextCache::draw(TTF_Font* font, const std::string& text, SDL_Color color, float x, float y,
                     Align align, Uint8 alpha) {
  if (!font || text.empty()) return;
  const Entry* e = get(font, text, color);
  if (!e->tex) return;
  float dx = x;
  if (align == Align::Center) dx = x - e->w / 2;
  if (align == Align::Right) dx = x - e->w;
  SDL_FRect dst{std::floor(dx), std::floor(y - e->h / 2), e->w, e->h};
  SDL_SetTextureAlphaMod(e->tex, alpha);
  SDL_RenderTexture(r_, e->tex, nullptr, &dst);
}

float TextCache::width(TTF_Font* font, const std::string& text) {
  if (!font || text.empty()) return 0;
  const Entry* e = get(font, text, SDL_Color{255, 255, 255, 255});
  return e->w;
}

ImageCache::~ImageCache() {
  for (auto& kv : map_)
    if (kv.second) SDL_DestroyTexture(kv.second);
}

SDL_Texture* ImageCache::get(const std::string& path) {
  auto it = map_.find(path);
  if (it != map_.end()) return it->second;
  SDL_Texture* tex = nullptr;
  if (SDL_Surface* s = SDL_LoadPNG(path.c_str())) {
    tex = SDL_CreateTextureFromSurface(r_, s);
    SDL_DestroySurface(s);
  } else {
    SDL_Log("warning: cannot load %s: %s", path.c_str(), SDL_GetError());
  }
  map_[path] = tex;
  return tex;
}

void fillRoundRect(SDL_Renderer* r, SDL_FRect rc, float radius, SDL_Color c, Uint8 alpha) {
  setColor(r, c, alpha == 255 ? c.a : alpha);
  radius = std::min({radius, rc.w / 2, rc.h / 2});
  if (radius < 1) {
    SDL_RenderFillRect(r, &rc);
    return;
  }
  const int rr = int(radius);
  // Middle band.
  SDL_FRect mid{rc.x, rc.y + rr, rc.w, rc.h - 2 * rr};
  SDL_RenderFillRect(r, &mid);
  for (int i = 0; i < rr; ++i) {
    const float dy = radius - (float(i) + 0.5f);
    const float inset = radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
    SDL_FRect top{rc.x + inset, rc.y + float(i), rc.w - 2 * inset, 1};
    SDL_FRect bot{rc.x + inset, rc.y + rc.h - 1 - float(i), rc.w - 2 * inset, 1};
    SDL_RenderFillRect(r, &top);
    SDL_RenderFillRect(r, &bot);
  }
}

void strokeRoundRect(SDL_Renderer* r, SDL_FRect rc, float radius, float t, SDL_Color c,
                     Uint8 alpha) {
  setColor(r, c, alpha == 255 ? c.a : alpha);
  radius = std::min({radius, rc.w / 2, rc.h / 2});
  SDL_FRect top{rc.x + radius, rc.y, rc.w - 2 * radius, t};
  SDL_FRect bot{rc.x + radius, rc.y + rc.h - t, rc.w - 2 * radius, t};
  SDL_FRect lef{rc.x, rc.y + radius, t, rc.h - 2 * radius};
  SDL_FRect rig{rc.x + rc.w - t, rc.y + radius, t, rc.h - 2 * radius};
  SDL_RenderFillRect(r, &top);
  SDL_RenderFillRect(r, &bot);
  SDL_RenderFillRect(r, &lef);
  SDL_RenderFillRect(r, &rig);
  // Corners: dense points on quarter arcs.
  std::vector<SDL_FPoint> pts;
  const float cxs[4] = {rc.x + radius, rc.x + rc.w - radius, rc.x + radius, rc.x + rc.w - radius};
  const float cys[4] = {rc.y + radius, rc.y + radius, rc.y + rc.h - radius,
                        rc.y + rc.h - radius};
  const float a0[4] = {180, 270, 90, 0};
  for (int k = 0; k < 4; ++k) {
    for (float rad = radius - t; rad <= radius; rad += 0.5f) {
      const int steps = std::max(8, int(rad * 2.2f));
      for (int s = 0; s <= steps; ++s) {
        const float ang = (a0[k] + 90.0f * float(s) / float(steps)) * 3.14159265f / 180.0f;
        pts.push_back({cxs[k] + std::cos(ang) * rad, cys[k] + std::sin(ang) * rad});
      }
    }
  }
  SDL_RenderPoints(r, pts.data(), int(pts.size()));
}

void strokeCircle(SDL_Renderer* r, float cx, float cy, float radius, float t, SDL_Color c,
                  Uint8 alpha) {
  setColor(r, c, alpha == 255 ? c.a : alpha);
  std::vector<SDL_FPoint> pts;
  for (float rad = radius - t; rad <= radius; rad += 0.5f) {
    const int steps = std::max(16, int(rad * 6.3f));
    for (int s = 0; s < steps; ++s) {
      const float ang = 6.2831853f * float(s) / float(steps);
      pts.push_back({cx + std::cos(ang) * rad, cy + std::sin(ang) * rad});
    }
  }
  SDL_RenderPoints(r, pts.data(), int(pts.size()));
}

void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c, Uint8 alpha) {
  setColor(r, c, alpha == 255 ? c.a : alpha);
  const int rr = int(radius);
  for (int i = -rr; i <= rr; ++i) {
    const float w = std::sqrt(std::max(0.0f, radius * radius - float(i) * float(i)));
    SDL_FRect row{cx - w, cy + float(i), 2 * w, 1};
    SDL_RenderFillRect(r, &row);
  }
}

void gradientRect(SDL_Renderer* r, SDL_FRect rc, SDL_Color top, SDL_Color bottom) {
  auto fc = [](SDL_Color c) { return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; };
  SDL_Vertex v[4] = {
      {{rc.x, rc.y}, fc(top), {0, 0}},
      {{rc.x + rc.w, rc.y}, fc(top), {0, 0}},
      {{rc.x + rc.w, rc.y + rc.h}, fc(bottom), {0, 0}},
      {{rc.x, rc.y + rc.h}, fc(bottom), {0, 0}},
  };
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

}  // namespace bac::app::gfx
