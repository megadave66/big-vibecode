// SDL renderer: sprites + SDL_ttf text, with flat-colour / debug-text fallbacks
// so a missing asset never stops the game.
#include "render.hpp"

#include <algorithm>
#include <cmath>

#include "core/config.hpp"

namespace flappy {

namespace {

namespace cfg = flappy::config;

// --- Visual-only layout numbers (gameplay constants live in config.hpp) ------
constexpr float kBgWorldH = static_cast<float>(cfg::kWorldHeight);
constexpr float kBirdDrawW = 36.0f;
constexpr float kBirdDrawH = 30.0f;
constexpr float kCapW = 58.0f;
constexpr float kCapH = 26.0f;
constexpr float kBirdFrameHz = 12.0f;
constexpr int kBirdFrozenFrame = 1;
constexpr float kScoreY = 40.0f;
constexpr float kTitleY = 120.0f;
constexpr float kHintY = 330.0f;
constexpr float kReadyBestY = 360.0f;
constexpr float kFlashSeconds = 0.1f;
constexpr float kPanelW = 244.0f;
constexpr float kPanelH = 196.0f;
constexpr float kPanelY = 150.0f;
constexpr float kPanelBorder = 4.0f;
constexpr float kOutlineOffset = 2.0f;
constexpr float kDebugCharW = 8.0f;
constexpr std::size_t kTextCacheMax = 48;

constexpr std::array<float, 4> kFontSizes{8.0f, 16.0f, 24.0f, 32.0f};
constexpr int kFontSmall = 0;
constexpr int kFontMedium = 1;
constexpr int kFontTitle = 2;
constexpr int kFontScore = 3;

struct Colour {
    Uint8 r, g, b;
};
constexpr Colour kWhite{255, 255, 255};
constexpr Colour kDark{54, 40, 30};
constexpr Colour kGold{255, 205, 60};
constexpr Colour kPanelFill{236, 222, 170};
constexpr Colour kPanelEdge{84, 56, 71};
constexpr Colour kFallbackSky{112, 197, 206};
constexpr Colour kFallbackPipe{84, 170, 40};
constexpr Colour kFallbackGround{222, 216, 149};
constexpr Colour kFallbackBird{250, 200, 40};

void fill_rect(SDL_Renderer* r, float x, float y, float w, float h, Colour c, Uint8 a = 255) {
    SDL_SetRenderDrawBlendMode(r, a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
    const SDL_FRect f{x, y, w, h};
    SDL_RenderFillRect(r, &f);
}

}  // namespace

SDL_Texture* Renderer::load_texture(const std::string& path) {
    SDL_Surface* s = SDL_LoadPNG(path.c_str());
    if (s == nullptr) {
        return nullptr;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_DestroySurface(s);
    return t;
}

bool Renderer::init(SDL_Renderer* renderer, const std::string& asset_dir) {
    renderer_ = renderer;
    if (renderer_ == nullptr) {
        return false;
    }
    background_ = load_texture(asset_dir + "sprites/background.png");
    for (std::size_t i = 0; i < bird_frames_.size(); ++i) {
        bird_frames_[i] = load_texture(asset_dir + "sprites/bird_" + std::to_string(i) + ".png");
    }
    pipe_body_ = load_texture(asset_dir + "sprites/pipe_body.png");
    pipe_cap_ = load_texture(asset_dir + "sprites/pipe_cap.png");
    if (background_ != nullptr) {
        bake_background();
    }
    ground_ = load_texture(asset_dir + "sprites/ground.png");

    ttf_ready_ = TTF_Init();
    if (ttf_ready_) {
        text_engine_ = TTF_CreateRendererTextEngine(renderer_);
        if (text_engine_ != nullptr) {
            for (std::size_t i = 0; i < fonts_.size(); ++i) {
                fonts_[i] = TTF_OpenFont((asset_dir + "fonts/ui.ttf").c_str(), kFontSizes[i]);
            }
        }
    }
    return true;
}

TTF_Text* Renderer::cached_text(int font_slot, const std::string& text) {
    for (const CachedText& c : text_cache_) {
        if (c.font_slot == font_slot && c.text == text) {
            return c.handle;
        }
    }
    if (text_cache_.size() >= kTextCacheMax) {
        clear_text_cache();  // scores grow without bound; start over
    }
    TTF_Text* t = TTF_CreateText(text_engine_, fonts_[static_cast<std::size_t>(font_slot)], text.c_str(), 0);
    if (t != nullptr) {
        text_cache_.push_back({font_slot, text, t});
    }
    return t;
}

void Renderer::clear_text_cache() {
    for (const CachedText& c : text_cache_) {
        TTF_DestroyText(c.handle);
    }
    text_cache_.clear();
}

void Renderer::draw_text(int font_slot, const std::string& text, float cx, float y, Rgb colour,
                         bool outline) {
    TTF_Font* font = text_engine_ != nullptr ? fonts_[static_cast<std::size_t>(font_slot)] : nullptr;
    if (font != nullptr) {
        TTF_Text* t = cached_text(font_slot, text);
        if (t != nullptr) {
            int w = 0;
            int h = 0;
            TTF_GetTextSize(t, &w, &h);
            const float x = std::round(cx - static_cast<float>(w) / 2.0f);
            if (outline) {
                TTF_SetTextColor(t, kDark.r, kDark.g, kDark.b, 255);
                const float o = kOutlineOffset * kFontSizes[static_cast<std::size_t>(font_slot)] / 16.0f;
                const float off = std::max(1.0f, std::round(o));
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx != 0 || dy != 0) {
                            TTF_DrawRendererText(t, x + static_cast<float>(dx) * off,
                                                 y + static_cast<float>(dy) * off);
                        }
                    }
                }
            }
            TTF_SetTextColor(t, colour.r, colour.g, colour.b, 255);
            TTF_DrawRendererText(t, x, y);
            return;
        }
    }
    // Fallback: built-in 8 px debug font, scaled to roughly match the slot.
    const float scale = std::max(1.0f, kFontSizes[static_cast<std::size_t>(font_slot)] / 8.0f);
    const float w = static_cast<float>(text.size()) * kDebugCharW * scale;
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_SetRenderDrawColor(renderer_, colour.r, colour.g, colour.b, 255);
    SDL_RenderDebugText(renderer_, (cx - w / 2.0f) / scale, y / scale, text.c_str());
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
}

// Scales the source art once into a world-sized target texture so each frame
// is a single 288x512 blit (the 800x480 art is costly on a software renderer).
void Renderer::bake_background() {
    float tw = 0.0f;
    float th = 0.0f;
    SDL_GetTextureSize(background_, &tw, &th);
    const float w = std::round(tw * kBgWorldH / th);
    background_baked_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                          cfg::kWorldWidth, cfg::kWorldHeight);
    if (background_baked_ == nullptr) {
        return;
    }
    SDL_SetTextureBlendMode(background_baked_, SDL_BLENDMODE_NONE);  // opaque: skip alpha blending
    SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, background_baked_);
    for (float x = 0.0f; x < static_cast<float>(cfg::kWorldWidth); x += w) {
        const SDL_FRect dst{x, 0.0f, w, kBgWorldH};
        SDL_RenderTexture(renderer_, background_, nullptr, &dst);
    }
    SDL_SetRenderTarget(renderer_, prev);
}

void Renderer::draw_background() {
    if (background_baked_ != nullptr) {
        SDL_RenderTexture(renderer_, background_baked_, nullptr, nullptr);
        return;
    }
    fill_rect(renderer_, 0, 0, static_cast<float>(cfg::kWorldWidth), kBgWorldH, kFallbackSky);
}

void Renderer::draw_pipes(const Game& game) {
    for (const Pipe& p : game.pipes().pipes()) {
        const Rect top = pipe_top_rect(p);
        const Rect bot = pipe_bottom_rect(p);
        const float cap_x = p.x + cfg::kPipeWidth / 2.0f - kCapW / 2.0f;
        for (const Rect& rc : {top, bot}) {
            if (rc.h <= 0.0f) {
                continue;
            }
            const SDL_FRect dst{rc.x, rc.y, rc.w, rc.h};
            if (pipe_body_ != nullptr) {
                SDL_RenderTexture(renderer_, pipe_body_, nullptr, &dst);
            } else {
                fill_rect(renderer_, rc.x, rc.y, rc.w, rc.h, kFallbackPipe);
            }
        }
        const SDL_FRect top_cap{cap_x, top.y + top.h - kCapH, kCapW, kCapH};
        const SDL_FRect bot_cap{cap_x, bot.y, kCapW, kCapH};
        if (pipe_cap_ != nullptr) {
            SDL_RenderTexture(renderer_, pipe_cap_, nullptr, &top_cap);
            SDL_RenderTexture(renderer_, pipe_cap_, nullptr, &bot_cap);
        } else {
            fill_rect(renderer_, top_cap.x, top_cap.y, top_cap.w, top_cap.h, kFallbackPipe);
            fill_rect(renderer_, bot_cap.x, bot_cap.y, bot_cap.w, bot_cap.h, kFallbackPipe);
        }
    }
}

void Renderer::draw_ground(const Game& game) {
    const float h = static_cast<float>(cfg::kGroundHeight);
    if (ground_ == nullptr) {
        fill_rect(renderer_, 0, cfg::kGroundY, static_cast<float>(cfg::kWorldWidth), h, kFallbackGround);
        return;
    }
    float tw = 0.0f;
    float th = 0.0f;
    SDL_GetTextureSize(ground_, &tw, &th);
    for (float x = -game.ground_offset(); x < static_cast<float>(cfg::kWorldWidth); x += tw) {
        const SDL_FRect dst{x, cfg::kGroundY, tw, h};
        SDL_RenderTexture(renderer_, ground_, nullptr, &dst);
    }
}

void Renderer::draw_bird(const Game& game) {
    const Bird& b = game.bird();
    const bool animate = game.state() == State::GetReady || game.state() == State::Playing;
    const int frame = animate ? static_cast<int>(game.state_time() * kBirdFrameHz) % 3 : kBirdFrozenFrame;
    SDL_Texture* tex = bird_frames_[static_cast<std::size_t>(frame)];
    const SDL_FRect dst{b.x - kBirdDrawW / 2.0f, b.y - kBirdDrawH / 2.0f, kBirdDrawW, kBirdDrawH};
    if (tex != nullptr) {
        SDL_RenderTextureRotated(renderer_, tex, nullptr, &dst, static_cast<double>(b.angle), nullptr,
                                 SDL_FLIP_NONE);
    } else {
        fill_rect(renderer_, dst.x, dst.y, dst.w, dst.h, kFallbackBird);
    }
}

void Renderer::draw_game_over_panel(const Game& game) {
    const float w = static_cast<float>(cfg::kWorldWidth);
    const float x = (w - kPanelW) / 2.0f;
    fill_rect(renderer_, x, kPanelY, kPanelW, kPanelH, kPanelEdge);
    fill_rect(renderer_, x + kPanelBorder, kPanelY + kPanelBorder, kPanelW - 2 * kPanelBorder,
              kPanelH - 2 * kPanelBorder, kPanelFill);
    // Rounded-look corners: notch the outer border.
    for (float cx : {x, x + kPanelW - kPanelBorder}) {
        for (float cy : {kPanelY, kPanelY + kPanelH - kPanelBorder}) {
            fill_rect(renderer_, cx, cy, kPanelBorder, kPanelBorder, kPanelFill);
        }
    }
    const float mid = w / 2.0f;
    draw_text(kFontTitle, "GAME OVER", mid, kPanelY + 22.0f, {kPanelEdge.r, kPanelEdge.g, kPanelEdge.b}, false);
    draw_text(kFontMedium, "SCORE " + std::to_string(game.score()), mid, kPanelY + 68.0f,
              {kPanelEdge.r, kPanelEdge.g, kPanelEdge.b}, false);
    draw_text(kFontMedium, "BEST " + std::to_string(game.best()), mid, kPanelY + 96.0f,
              {kPanelEdge.r, kPanelEdge.g, kPanelEdge.b}, false);
    if (game.new_best()) {
        draw_text(kFontMedium, "NEW BEST!", mid, kPanelY + 124.0f, {200, 60, 40}, false);
    }
    if (game.state_time() >= cfg::kGameOverInputDelay) {
        draw_text(kFontSmall, "PRESS SPACE", mid, kPanelY + 162.0f, {kPanelEdge.r, kPanelEdge.g, kPanelEdge.b},
                  false);
    }
}

void Renderer::draw_ui(const Game& game) {
    const float mid = static_cast<float>(cfg::kWorldWidth) / 2.0f;
    switch (game.state()) {
        case State::GetReady:
            draw_text(kFontTitle, "GET READY", mid, kTitleY, {kGold.r, kGold.g, kGold.b}, true);
            draw_text(kFontSmall, "SPACE / CLICK TO FLAP", mid, kHintY, {kWhite.r, kWhite.g, kWhite.b}, true);
            draw_text(kFontSmall, "BEST " + std::to_string(game.best()), mid, kReadyBestY,
                      {kWhite.r, kWhite.g, kWhite.b}, true);
            break;
        case State::Playing:
        case State::Dying:
            draw_text(kFontScore, std::to_string(game.score()), mid, kScoreY, {kWhite.r, kWhite.g, kWhite.b},
                      true);
            break;
        case State::GameOver:
            draw_text(kFontScore, std::to_string(game.score()), mid, kScoreY, {kWhite.r, kWhite.g, kWhite.b},
                      true);
            draw_game_over_panel(game);
            break;
    }
}

void Renderer::draw(const Game& game) {
    if (renderer_ == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    draw_background();
    draw_pipes(game);
    draw_ground(game);
    draw_bird(game);
    draw_ui(game);
    if (game.state() == State::Dying && game.state_time() < kFlashSeconds) {
        const float a = 1.0f - game.state_time() / kFlashSeconds;
        fill_rect(renderer_, 0, 0, static_cast<float>(cfg::kWorldWidth), kBgWorldH, kWhite,
                  static_cast<Uint8>(a * 200.0f));
    }
}

void Renderer::shutdown() {
    clear_text_cache();
    for (SDL_Texture** t : {&background_, &background_baked_, &pipe_body_, &pipe_cap_, &ground_}) {
        if (*t != nullptr) {
            SDL_DestroyTexture(*t);
            *t = nullptr;
        }
    }
    for (SDL_Texture*& t : bird_frames_) {
        if (t != nullptr) {
            SDL_DestroyTexture(t);
            t = nullptr;
        }
    }
    for (TTF_Font*& f : fonts_) {
        if (f != nullptr) {
            TTF_CloseFont(f);
            f = nullptr;
        }
    }
    if (text_engine_ != nullptr) {
        TTF_DestroyRendererTextEngine(text_engine_);
        text_engine_ = nullptr;
    }
    if (ttf_ready_) {
        TTF_Quit();
        ttf_ready_ = false;
    }
    renderer_ = nullptr;
}

}  // namespace flappy
