// App entry: SDL window, fixed-timestep loop, headless smoke and screenshot modes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "audio.hpp"
#include "core/autopilot.hpp"
#include "core/best_score.hpp"
#include "core/config.hpp"
#include "core/game.hpp"
#include "render.hpp"

namespace {

using namespace flappy;
namespace cfg = flappy::config;

struct Options {
    long smoke_ticks = 0;
    std::string screenshot_path;
    long frames = 400;
    std::uint32_t seed = 0;
    bool seed_set = false;
    std::string best_file;
    std::string script = "auto";  // auto | idle | die
};

bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_val = i + 1 < argc;
        if (a == "--smoke" && has_val) {
            o.smoke_ticks = std::strtol(argv[++i], nullptr, 10);
        } else if (a == "--screenshot" && has_val) {
            o.screenshot_path = argv[++i];
        } else if (a == "--frames" && has_val) {
            o.frames = std::strtol(argv[++i], nullptr, 10);
        } else if (a == "--seed" && has_val) {
            o.seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
            o.seed_set = true;
        } else if (a == "--best-file" && has_val) {
            o.best_file = argv[++i];
        } else if (a == "--script" && has_val) {
            o.script = argv[++i];
            if (o.script != "auto" && o.script != "idle" && o.script != "die") {
                std::fprintf(stderr, "unknown --script mode: %s\n", o.script.c_str());
                return false;
            }
        } else {
            std::fprintf(stderr,
                         "usage: flappy [--smoke N] [--screenshot PATH --frames N] [--seed S] "
                         "[--best-file PATH] [--script auto|idle|die]\n");
            return false;
        }
    }
    return true;
}

// Handles events common to every mode. Returns false when the app should quit.
bool pump_events(Game& game, bool interactive) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                return false;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) {
                    return false;
                }
                if (interactive && !e.key.repeat &&
                    (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP || e.key.key == SDLK_W)) {
                    game.press();
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (interactive && e.button.button == SDL_BUTTON_LEFT && e.button.which != SDL_TOUCH_MOUSEID) {
                    game.press();
                }
                break;
            case SDL_EVENT_FINGER_DOWN:
                if (interactive) {
                    game.press();
                }
                break;
            default:
                break;
        }
    }
    return true;
}

void handle_events(Game& game, Audio& audio, const std::string& best_file) {
    const Events ev = game.take_events();
    if (ev.flapped) {
        audio.play_flap();
    }
    if (ev.scored) {
        audio.play_score();
    }
    if (ev.hit) {
        audio.play_hit();
    }
    if (ev.game_over && game.new_best() && !best_file.empty()) {
        save_best(best_file, game.best());
    }
}

constexpr int kDieScriptScore = 3;  // --script die stops flapping at this score

// Scripted player used by --smoke and --screenshot.
//   auto: autopilot forever, restarts after game over.
//   idle: never presses (stays on the get-ready screen).
//   die:  starts, flies until the score reaches kDieScriptScore, then stops
//         pressing so the bird dies; never restarts.
void scripted_input(Game& game, const std::string& mode) {
    if (mode == "idle") {
        return;
    }
    if (mode == "die") {
        if (game.state() == State::GetReady) {
            game.press();
        } else if (game.state() == State::Playing && game.score() < kDieScriptScore &&
                   autopilot_should_flap(game.bird(), game.pipes())) {
            game.press();
        }
        return;
    }
    switch (game.state()) {
        case State::GetReady:
            game.press();
            break;
        case State::Playing:
            if (autopilot_should_flap(game.bird(), game.pipes())) {
                game.press();
            }
            break;
        case State::GameOver:
            if (game.state_time() > cfg::kGameOverInputDelay) {
                game.press();
            }
            break;
        case State::Dying:
            break;
    }
}

bool save_screenshot(SDL_Renderer* r, const std::string& path) {
    SDL_Surface* s = SDL_RenderReadPixels(r, nullptr);
    if (s == nullptr) {
        return false;
    }
    const bool ok = SDL_SavePNG(s, path.c_str());
    SDL_DestroySurface(s);
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        return 2;
    }
    const bool headless = opt.smoke_ticks > 0 || !opt.screenshot_path.empty();
    const long scripted_ticks = opt.smoke_ticks > 0 ? opt.smoke_ticks : opt.frames;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool audio_ok = SDL_InitSubSystem(SDL_INIT_AUDIO);  // failure is not fatal

    if (!opt.seed_set) {
        opt.seed = static_cast<std::uint32_t>(SDL_GetTicksNS());
    }
    if (opt.best_file.empty()) {
        char* pref = SDL_GetPrefPath("big-vibecode", "flappy-remake");
        if (pref != nullptr) {
            opt.best_file = std::string(pref) + "best.txt";
            SDL_free(pref);
        } else {
            SDL_Log("warning: no preferences folder; best score will not be saved");
        }
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    // Headless runs use a 1x window: the dummy driver renders in software, and the
    // 2x logical-presentation upscale alone costs ~20 ms per frame there.
    const int win_scale = headless ? 1 : cfg::kWindowScale;
    if (!SDL_CreateWindowAndRenderer("Flappy Remake", cfg::kWorldWidth * win_scale,
                                     cfg::kWorldHeight * win_scale, SDL_WINDOW_RESIZABLE,
                                     &window, &sdl_renderer)) {
        std::fprintf(stderr, "window/renderer creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(sdl_renderer, cfg::kWorldWidth, cfg::kWorldHeight,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (!headless) {
        SDL_SetRenderVSync(sdl_renderer, 1);
    }

    const char* base = SDL_GetBasePath();
    const std::string asset_dir = std::string(base != nullptr ? base : "") + "assets/";

    Renderer renderer;
    if (!renderer.init(sdl_renderer, asset_dir)) {
        std::fprintf(stderr, "renderer init failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    Audio audio;
    if (audio_ok) {
        audio.init(asset_dir);  // ignore failure: the game runs silent
    }

    Game game(opt.seed, opt.best_file.empty() ? 0 : load_best(opt.best_file));
    int exit_code = 0;

    if (headless) {
        bool quit = false;
        for (long i = 0; i < scripted_ticks && !quit; ++i) {
            quit = !pump_events(game, false);
            scripted_input(game, opt.script);
            game.tick(cfg::kDt);
            handle_events(game, audio, opt.best_file);
            if (i + 1 < scripted_ticks) {
                renderer.draw(game);
                SDL_RenderPresent(sdl_renderer);
            }
        }
        renderer.draw(game);
        if (!opt.screenshot_path.empty()) {
            if (!save_screenshot(sdl_renderer, opt.screenshot_path)) {
                std::fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
                exit_code = 1;
            }
        }
        SDL_RenderPresent(sdl_renderer);
        if (opt.smoke_ticks > 0 && exit_code == 0) {
            std::printf("SMOKE OK ticks=%ld score=%d best=%d state=%d\n", opt.smoke_ticks,
                        game.score(), game.best(), static_cast<int>(game.state()));
        }
    } else {
        bool running = true;
        double acc = 0.0;
        std::uint64_t last = SDL_GetTicksNS();
        while (running) {
            running = pump_events(game, true);
            const std::uint64_t now = SDL_GetTicksNS();
            double frame = static_cast<double>(now - last) / 1e9;
            last = now;
            if (frame > static_cast<double>(cfg::kMaxFrameTime)) {
                frame = static_cast<double>(cfg::kMaxFrameTime);
            }
            acc += frame;
            while (acc >= static_cast<double>(cfg::kDt)) {
                game.tick(cfg::kDt);
                handle_events(game, audio, opt.best_file);
                acc -= static_cast<double>(cfg::kDt);
            }
            renderer.draw(game);
            SDL_RenderPresent(sdl_renderer);
        }
    }

    audio.shutdown();
    renderer.shutdown();
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
