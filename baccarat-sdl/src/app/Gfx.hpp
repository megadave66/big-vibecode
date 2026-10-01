#pragma once
// SDL drawing helpers: text cache, image cache, rounded rectangles, gradients.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <unordered_map>

#include "Layout.hpp"

namespace bac::app::gfx {

enum class Align { Left, Center, Right };

inline SDL_FRect toF(const layout::Rect& r) { return SDL_FRect{r.x, r.y, r.w, r.h}; }
inline void setColor(SDL_Renderer* r, SDL_Color c, Uint8 a = 255) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a == 255 ? c.a : a);
}

// Caches rendered text textures keyed by font + colour + string.
class TextCache {
 public:
  explicit TextCache(SDL_Renderer* r) : r_(r) {}
  ~TextCache();
  TextCache(const TextCache&) = delete;
  TextCache& operator=(const TextCache&) = delete;

  // (x, y) = anchor point; vertically centred on y. Null font -> no-op.
  void draw(TTF_Font* font, const std::string& text, SDL_Color color, float x, float y,
            Align align = Align::Left, Uint8 alpha = 255);
  float width(TTF_Font* font, const std::string& text);
  size_t size() const { return map_.size(); }

 private:
  struct Entry {
    SDL_Texture* tex = nullptr;
    float w = 0, h = 0;
  };
  const Entry* get(TTF_Font* font, const std::string& text, SDL_Color color);
  SDL_Renderer* r_;
  std::unordered_map<std::string, Entry> map_;
};

// Loads PNG textures lazily. A missing file yields null (remembered).
class ImageCache {
 public:
  explicit ImageCache(SDL_Renderer* r) : r_(r) {}
  ~ImageCache();
  ImageCache(const ImageCache&) = delete;
  ImageCache& operator=(const ImageCache&) = delete;
  SDL_Texture* get(const std::string& path);

 private:
  SDL_Renderer* r_;
  std::unordered_map<std::string, SDL_Texture*> map_;
};

void fillRoundRect(SDL_Renderer* r, SDL_FRect rc, float radius, SDL_Color c, Uint8 alpha = 255);
void strokeRoundRect(SDL_Renderer* r, SDL_FRect rc, float radius, float thickness, SDL_Color c,
                     Uint8 alpha = 255);
void strokeCircle(SDL_Renderer* r, float cx, float cy, float radius, float thickness,
                  SDL_Color c, Uint8 alpha = 255);
void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c, Uint8 alpha = 255);
// Vertical gradient filling the whole rect.
void gradientRect(SDL_Renderer* r, SDL_FRect rc, SDL_Color top, SDL_Color bottom);

}  // namespace bac::app::gfx
