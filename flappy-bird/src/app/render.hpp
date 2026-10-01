// SDL rendering layer. Draws a Game; never changes it. Deterministic:
// nothing here reads the wall clock.
#pragma once

#include <array>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "core/game.hpp"

namespace flappy {

class Renderer {
public:
    bool init(SDL_Renderer* renderer, const std::string& asset_dir);
    void draw(const Game& game);
    void shutdown();

private:
    struct Rgb {
        Uint8 r, g, b;
    };

    SDL_Texture* load_texture(const std::string& path);
    void draw_text(int font_slot, const std::string& text, float cx, float y, Rgb colour, bool outline);
    void draw_background();
    void draw_pipes(const Game& game);
    void draw_ground(const Game& game);
    void draw_bird(const Game& game);
    void draw_ui(const Game& game);
    void draw_game_over_panel(const Game& game);

    struct CachedText {
        int font_slot;
        std::string text;
        TTF_Text* handle;
    };
    TTF_Text* cached_text(int font_slot, const std::string& text);
    void clear_text_cache();
    void bake_background();

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* background_baked_ = nullptr;  // world-sized copy of the scaled background
    std::vector<CachedText> text_cache_;
    SDL_Texture* background_ = nullptr;
    std::array<SDL_Texture*, 3> bird_frames_{};
    SDL_Texture* pipe_body_ = nullptr;
    SDL_Texture* pipe_cap_ = nullptr;
    SDL_Texture* ground_ = nullptr;
    bool ttf_ready_ = false;
    TTF_TextEngine* text_engine_ = nullptr;
    std::array<TTF_Font*, 4> fonts_{};  // sizes in render.cpp: 8, 16, 24, 32 px
};

}  // namespace flappy
