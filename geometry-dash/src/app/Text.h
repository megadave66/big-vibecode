#pragma once
// Cached text drawing with SDL_ttf. One instance is shared by the game and the menus.
//   TextRenderer text; text.init(renderer, fontPath);
//   text.draw("Hello", x, y, 32, color, TextAlign::Center);
// Sizes are in logical pixels (the 1280x720 canvas). Textures are cached per (size, string) and
// tinted with colour mod, so colour changes are free.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <list>
#include <string>
#include <unordered_map>

#include "Gfx.h"

namespace gd {

enum class TextAlign { Left, Center, Right };

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    // Returns false if the font cannot be loaded; draw() is then a no-op.
    bool init(SDL_Renderer* renderer, const std::string& fontPath);
    void shutdown();
    bool ready() const { return renderer_ != nullptr && !fontPath_.empty(); }

    // Output pixels per logical pixel (for sharp text on high-DPI / resized windows).
    void setPixelScale(float scale);

    // y is the top of the text line. Colour alpha applies. `shadow` draws a dark offset copy.
    void draw(Gfx& gfx, const std::string& s, float x, float y, int size, Rgba color,
              TextAlign align = TextAlign::Left, bool shadow = true);
    // Width / height in logical pixels.
    void measure(const std::string& s, int size, float& w, float& h);

private:
    struct Entry {
        SDL_Texture* tex = nullptr;
        float w = 0, h = 0;  // logical size
    };
    TTF_Font* font(int size);
    Entry* entry(const std::string& s, int size);
    void clearCache();

    SDL_Renderer* renderer_ = nullptr;
    std::string fontPath_;
    float scale_ = 1.0f;
    bool ttfInit_ = false;
    std::unordered_map<int, TTF_Font*> fonts_;
    std::unordered_map<std::string, Entry> cache_;
};

}  // namespace gd
