#include "App.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <utility>

#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"
#include "core/Replay.h"

namespace gd {

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;
constexpr long kBootFrames = 1;
constexpr int kMaxSequenceShotsLimit = 200;
constexpr long kScriptStep = 6;   // frames between --menu-script keys

bool needValue(int i, int argc, const char* flag, std::string& error) {
    if (i + 1 >= argc) {
        error = std::string("missing value for ") + flag;
        return false;
    }
    return true;
}

bool parseInt(const char* s, int& out) {
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(item);
    return out;
}

}  // namespace

bool parseArgs(int argc, char** argv, AppOptions& out, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data") {
            if (!needValue(i, argc, "--data", error)) return false;
            out.dataDir = argv[++i];
        } else if (a == "--level") {
            if (!needValue(i, argc, "--level", error)) return false;
            if (!parseInt(argv[++i], out.level) || out.level < 1 || out.level > 10) {
                error = "--level needs an integer 1..10";
                return false;
            }
        } else if (a == "--level-file") {
            if (!needValue(i, argc, "--level-file", error)) return false;
            out.levelFile = argv[++i];
        } else if (a == "--practice") {
            out.practice = true;
        } else if (a == "--fly") {
            out.fly = true;
        } else if (a == "--hitboxes") {
            out.hitboxes = true;
        } else if (a == "--replay") {
            if (!needValue(i, argc, "--replay", error)) return false;
            out.replayFile = argv[++i];
        } else if (a == "--smoke") {
            if (!needValue(i, argc, "--smoke", error)) return false;
            if (!parseInt(argv[++i], out.smokeFrames) || out.smokeFrames < 0) {
                error = "--smoke needs a non-negative frame count";
                return false;
            }
        } else if (a == "--screenshot") {
            if (!needValue(i, argc, "--screenshot", error)) return false;
            out.screenshotFile = argv[++i];
        } else if (a == "--frame") {
            if (!needValue(i, argc, "--frame", error)) return false;
            if (!parseInt(argv[++i], out.screenshotFrame) || out.screenshotFrame < 0) {
                error = "--frame needs a non-negative integer";
                return false;
            }
        } else if (a == "--frame-every") {
            if (!needValue(i, argc, "--frame-every", error)) return false;
            if (!parseInt(argv[++i], out.frameEvery) || out.frameEvery < 1) {
                error = "--frame-every needs a positive integer";
                return false;
            }
        } else if (a == "--max-shots") {
            if (!needValue(i, argc, "--max-shots", error)) return false;
            if (!parseInt(argv[++i], out.maxShots) || out.maxShots < 1 || out.maxShots > kMaxSequenceShotsLimit) {
                error = "--max-shots needs an integer 1..200";
                return false;
            }
        } else if (a == "--screenshot-dir") {
            if (!needValue(i, argc, "--screenshot-dir", error)) return false;
            out.screenshotDir = argv[++i];
        } else if (a == "--hold") {
            if (!needValue(i, argc, "--hold", error)) return false;
            for (const std::string& part : splitComma(argv[++i])) {
                int lo = 0, hi = 0;
                if (std::sscanf(part.c_str(), "%d-%d", &lo, &hi) != 2 || lo < 0 || hi < lo) {
                    error = "--hold needs ranges like 60-75,200-215";
                    return false;
                }
                out.holdRanges.emplace_back(lo, hi);
            }
        } else if (a == "--checkpoint-at") {
            if (!needValue(i, argc, "--checkpoint-at", error)) return false;
            for (const std::string& part : splitComma(argv[++i])) {
                int f = 0;
                if (!parseInt(part.c_str(), f) || f < 0) {
                    error = "--checkpoint-at needs frame numbers like 100,150";
                    return false;
                }
                out.checkpointFrames.push_back(f);
            }
        } else if (a == "--save") {
            if (!needValue(i, argc, "--save", error)) return false;
            out.savePath = argv[++i];
        } else if (a == "--menu-script") {
            if (!needValue(i, argc, "--menu-script", error)) return false;
            out.menuScript = splitComma(argv[++i]);
        } else if (a == "--mute") {
            out.mute = true;
        } else {
            error = "unknown argument: " + a;
            return false;
        }
    }
    if (out.frameEvery > 0 && out.screenshotDir.empty()) {
        error = "--frame-every needs --screenshot-dir";
        return false;
    }
    return true;
}

App::App(AppOptions options) : opt_(std::move(options)) {}

App::~App() { shutdown(); }

bool App::init() {
    dataDir_ = opt_.dataDir;
    if (dataDir_.empty()) {
        if (const char* env = std::getenv("GD_DATA_DIR")) dataDir_ = env;
    }
#ifdef GD_DATA_DIR
    if (dataDir_.empty()) dataDir_ = GD_DATA_DIR;
#endif
    if (dataDir_.empty()) dataDir_ = ".";

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    // Audio is optional: a missing device must not stop the game (or the headless smoke run).
    if (!opt_.mute && !SDL_InitSubSystem(SDL_INIT_AUDIO))
        std::fprintf(stderr, "warning: audio init failed: %s\n", SDL_GetError());

    window_ = SDL_CreateWindow("Geometry Dash Remake", kWindowW, kWindowH, SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    const char* video = SDL_GetCurrentVideoDriver();
    const bool headless = video && std::strcmp(video, "dummy") == 0;
    renderer_ = SDL_CreateRenderer(window_, headless ? "software" : nullptr);
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer_, kWindowW, kWindowH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (!headless) SDL_SetRenderVSync(renderer_, 1);

    gfx_ = std::make_unique<Gfx>(renderer_);
    if (!text_.init(renderer_, dataDir_ + "/assets/fonts/RussoOne-Regular.ttf"))
        std::fprintf(stderr, "warning: text disabled (font not loaded)\n");
    audio_.init(dataDir_, opt_.mute);
    levels_ = menu::loadLevelInfos(dataDir_);
    practiceSelected_ = opt_.practice;
    return true;
}

void App::shutdown() {
    saveProgress();
    game_.reset();
    audio_.shutdown();
    text_.shutdown();
    gfx_.reset();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
    SDL_Quit();
}

bool App::startGame(int levelId) { return startGameFile(levelPath(dataDir_, levelId)); }

bool App::startGameFile(const std::string& path) {
    GameConfig cfg;
    cfg.dataDir = dataDir_;
    cfg.levelFile = path;
    cfg.practice = practiceSelected_;
    // Smoke runs that go straight into a level fly through it so the whole level is exercised.
    cfg.fly = opt_.fly || (opt_.smokeFrames >= 0 && directStart_);
    if (!opt_.replayFile.empty()) {
        std::vector<std::string> errs;
        auto rp = loadReplayFromFile(opt_.replayFile, errs);
        if (!rp) {
            error_ = "cannot load replay " + opt_.replayFile;
            for (const auto& e : errs) error_ += "\n  " + e;
            return false;
        }
        cfg.useReplay = true;
        cfg.replayHeld = heldPerTick(*rp);
    }
    auto g = std::make_unique<GameScreen>(&audio_);
    std::string err;
    if (!g->load(cfg, err)) {
        error_ = err;
        return false;
    }
    game_ = std::move(g);
    pendingPause_ = false;
    attachProgressHooks();
    timestep_.reset();
    stateFrames_ = 0;
    // Boot/menus -> Playing (Boot has to pass through MainMenu in the transition table).
    if (state_ == AppState::Boot) changeState(AppState::MainMenu);
    if (state_ != AppState::Playing) changeState(AppState::Playing);
    return state_ == AppState::Playing;
}

void App::attachProgressHooks() {
    // Debug fly mode cannot die, so it never counts.
    if (!game_ || game_->fly()) return;
    const int id = game_->level().id;
    progress_.recordAttempt(id);   // the first attempt; later ones come from onAttempt
    GameHooks& h = game_->hooks();
    h.onAttempt = [this, id] { progress_.recordAttempt(id); };
    h.onDeath = [this, id](const GameResult& r) {
        progress_.recordBest(id, r.practice, static_cast<int>(std::floor(r.progress * 100.0)));
        saveProgress();
    };
    h.onComplete = [this, id](const GameResult& r) {
        progress_.recordWin(id, r.practice);
        saveProgress();
    };
}

void App::saveProgress() {
    if (!progress_.dirty()) return;
    if (!progress_.save()) std::fprintf(stderr, "warning: cannot save progress to %s\n", progress_.path().c_str());
    progress_.clearDirty();
}

void App::leaveGame() {
    audio_.stopMusic(0);
    game_.reset();
}

void App::pollInput(InputState& in) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: in.quit = true; break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.repeat) break;
                switch (e.key.key) {
                    case SDLK_SPACE:
                    case SDLK_UP: in.held = true; in.up = true; break;
                    case SDLK_ESCAPE: in.pause = true; break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER: in.confirm = true; break;
                    case SDLK_DOWN: in.down = true; break;
                    case SDLK_LEFT: in.left = true; break;
                    case SDLK_RIGHT: in.right = true; break;
                    case SDLK_Z:
                    case SDLK_C: in.checkpoint = true; break;
                    case SDLK_X: in.removeCheckpoint = true; break;
                    case SDLK_R: in.restart = true; break;
                    case SDLK_P: in.togglePractice = true; break;
                    default: break;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                SDL_ConvertEventToRenderCoordinates(renderer_, &e);
                in.mouseMoved = true;
                in.mouseX = e.motion.x;
                in.mouseY = e.motion.y;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                SDL_ConvertEventToRenderCoordinates(renderer_, &e);
                if (e.button.button == SDL_BUTTON_LEFT) {
                    in.held = true;
                    in.click = true;
                    in.mouseMoved = true;
                    in.mouseX = e.button.x;
                    in.mouseY = e.button.y;
                }
                break;
            default: break;
        }
    }
    // "held" is a level: also true while a mapped key / button is still down.
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP]) in.held = true;
    if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) in.held = true;
}

void App::scriptedInput(InputState& in) const {
    // Headless scripted runs. Explicit --hold ranges win; otherwise a straight smoke run into a
    // level taps the jump key on a fixed rhythm; a smoke run without a level walks the menus.
    for (const auto& r : opt_.holdRanges)
        if (frame_ >= r.first && frame_ <= r.second) in.held = true;
    for (int f : opt_.checkpointFrames)
        if (frame_ == f) in.checkpoint = true;

    if (!opt_.menuScript.empty()) {
        // One token every kScriptStep frames, starting at frame kScriptStep.
        if (frame_ >= kScriptStep && frame_ % kScriptStep == 0) {
            const std::size_t idx = static_cast<std::size_t>(frame_ / kScriptStep - 1);
            if (idx < opt_.menuScript.size()) {
                const std::string& tok = opt_.menuScript[idx];
                if (tok == "up") in.up = true;
                else if (tok == "down") in.down = true;
                else if (tok == "left") in.left = true;
                else if (tok == "right") in.right = true;
                else if (tok == "enter") in.confirm = true;
                else if (tok == "esc") in.pause = true;
                else if (tok == "p") in.togglePractice = true;
                else if (tok == "r") in.restart = true;
                else if (tok == "space") in.held = true;
                else if (tok.rfind("mouse:", 0) == 0 || tok.rfind("click:", 0) == 0) {
                    float x = 0, y = 0;
                    if (std::sscanf(tok.c_str() + tok.find(':') + 1, "%f:%f", &x, &y) == 2) {
                        in.mouseMoved = true;
                        in.mouseX = x;
                        in.mouseY = y;
                        in.click = tok[0] == 'c';
                    }
                }
            }
        }
        return;
    }
    if (state_ == AppState::Playing && opt_.holdRanges.empty() && opt_.replayFile.empty() &&
        opt_.smokeFrames >= 0 && directStart_) {
        if (frame_ % 36 < 9) in.held = true;
        if (opt_.practice && frame_ % 400 == 399) in.checkpoint = true;
        return;
    }
    if (opt_.smokeFrames >= 0 && !directStart_) {
        switch (frame_) {
            case 10: in.confirm = true; break;   // MainMenu -> LevelSelect
            case 20: in.confirm = true; break;   // LevelSelect -> Playing
            case 40: in.pause = true; break;     // Playing -> Paused
            case 50: in.pause = true; break;     // Paused -> Playing
            case 60: in.held = true; break;
            default: break;
        }
    }
}

void App::changeState(AppState to) {
    const AppState from = state_;
    if (!transition(state_, to)) return;
    stateFrames_ = 0;
    if (to == AppState::Paused) pauseIndex_ = 0;
    if (to == AppState::Complete) completeIndex_ = 0;
    // Save whenever play stops (death and win already saved; this catches pause and quit).
    if (from == AppState::Playing && to != AppState::Dead) saveProgress();
}

namespace {
int hitIndex(const InputState& in, int n, menu::Rect (*rect)(int)) {
    if (!in.mouseMoved) return -1;
    for (int i = 0; i < n; ++i)
        if (rect(i).contains(in.mouseX, in.mouseY)) return i;
    return -1;
}
}  // namespace

void App::activateMain(int index) {
    audio_.playSfx(Sfx::MenuSelect);
    if (index == 0) {
        changeState(AppState::LevelSelect);
        toast_.clear();
    } else {
        changeState(AppState::Quit);
    }
}

void App::activateLevel(int index) {
    const bool ok = index >= 0 && index < static_cast<int>(levels_.size()) &&
                    levels_[static_cast<std::size_t>(index)].available;
    if (!ok) {
        toast_ = "Level " + std::to_string(index + 1) + " is missing";
        return;
    }
    audio_.playSfx(Sfx::MenuSelect);
    if (!startGame(index + 1)) toast_ = "Cannot load level " + std::to_string(index + 1);
    else toast_.clear();
}

void App::activatePause(int index) {
    if (!game_) return;
    audio_.playSfx(Sfx::MenuSelect);
    switch (index) {
        case 0:  // Resume
            break;
        case 1:  // Restart
            game_->setPaused(false);
            game_->restart();
            break;
        case 2:  // Practice toggle (restarts the run)
            game_->setPaused(false);
            game_->setPractice(!game_->practiceMode());
            practiceSelected_ = game_->practiceMode();
            break;
        default:  // Quit to menu
            leaveGame();
            changeState(AppState::MainMenu);
            return;
    }
    game_->setPaused(false);
    timestep_.reset();
    changeState(AppState::Playing);
}

void App::activateComplete(int index) {
    if (!game_) return;
    audio_.playSfx(Sfx::MenuSelect);
    if (index == 1) {
        game_->restart();
        timestep_.reset();
        changeState(AppState::Playing);
    } else {
        leaveGame();
        changeState(AppState::LevelSelect);
    }
}

void App::updateLevelSelect(const InputState& in) {
    const int before = levelIndex_;
    constexpr int n = menu::kLevelCount;
    if (in.up && levelIndex_ >= menu::kLevelColumns) levelIndex_ -= menu::kLevelColumns;
    if (in.down && levelIndex_ + menu::kLevelColumns < n) levelIndex_ += menu::kLevelColumns;
    if (in.left && levelIndex_ > 0) --levelIndex_;
    if (in.right && levelIndex_ < n - 1) ++levelIndex_;
    const int hover = hitIndex(in, n, menu::levelCard);
    if (hover >= 0) levelIndex_ = hover;
    if (before != levelIndex_) audio_.playSfx(Sfx::MenuMove);

    if (in.togglePractice) {
        practiceSelected_ = !practiceSelected_;
        audio_.playSfx(Sfx::MenuMove);
    }
    if (in.click && in.mouseMoved) {
        if (menu::practiceButton().contains(in.mouseX, in.mouseY)) {
            practiceSelected_ = !practiceSelected_;
            audio_.playSfx(Sfx::MenuMove);
            return;
        }
        if (menu::backButton().contains(in.mouseX, in.mouseY)) {
            audio_.playSfx(Sfx::MenuSelect);
            changeState(AppState::MainMenu);
            return;
        }
        if (hover >= 0) {
            activateLevel(hover);
            return;
        }
    }
    if (in.confirm) activateLevel(levelIndex_);
    else if (in.pause) changeState(AppState::MainMenu);
}

void App::updatePause(const InputState& in) {
    if (!game_) return;
    const int before = pauseIndex_;
    if (in.up && pauseIndex_ > 0) --pauseIndex_;
    if (in.down && pauseIndex_ < menu::kPauseButtons - 1) ++pauseIndex_;
    const int hover = hitIndex(in, menu::kPauseButtons, menu::pauseButton);
    if (hover >= 0) pauseIndex_ = hover;
    if (before != pauseIndex_) audio_.playSfx(Sfx::MenuMove);

    if (in.pause) {
        activatePause(0);
    } else if (in.restart) {
        activatePause(1);
    } else if (in.togglePractice) {
        activatePause(2);
    } else if (in.click && hover >= 0) {
        activatePause(hover);
    } else if (in.confirm) {
        activatePause(pauseIndex_);
    }
}

void App::updateComplete(const InputState& in) {
    if (!game_) return;
    const int before = completeIndex_;
    if (in.left || in.up) completeIndex_ = 0;
    if (in.right || in.down) completeIndex_ = 1;
    const int hover = hitIndex(in, menu::kCompleteButtons, menu::completeButton);
    if (hover >= 0) completeIndex_ = hover;
    if (before != completeIndex_) audio_.playSfx(Sfx::MenuMove);

    if (in.restart) activateComplete(1);
    else if (in.click && hover >= 0) activateComplete(hover);
    else if (in.confirm) activateComplete(completeIndex_);
    else if (in.pause) activateComplete(0);
}

void App::updateDead(const InputState& in) {
    // The game screen runs the short death burst and restarts the level by itself. Dead only
    // lasts that moment; a pause request made during it is kept for when play resumes.
    if (!game_) return;
    game_->feedInput(in.held);
    if (in.pause) pendingPause_ = true;
    if (in.restart) {
        game_->restart();
        changeState(AppState::Playing);
    }
}

void App::updateMenus(const InputState& in) {
    if (state_ == AppState::MainMenu) {
        const int before = menuIndex_;
        if (in.up || in.left) menuIndex_ = std::max(0, menuIndex_ - 1);
        if (in.down || in.right) menuIndex_ = std::min(menu::kMainButtons - 1, menuIndex_ + 1);
        const int hover = hitIndex(in, menu::kMainButtons, menu::mainButton);
        if (hover >= 0) menuIndex_ = hover;
        if (before != menuIndex_) audio_.playSfx(Sfx::MenuMove);
        if (in.click && hover >= 0) activateMain(hover);
        else if (in.confirm) activateMain(menuIndex_);
        else if (in.pause) changeState(AppState::Quit);
    } else if (state_ == AppState::LevelSelect) {
        updateLevelSelect(in);
    }
}

void App::update(const InputState& in) {
    ++stateFrames_;
    switch (state_) {
        case AppState::Boot:
            if (stateFrames_ >= kBootFrames) {
                if (directStart_) {
                    const bool ok = !opt_.levelFile.empty() ? startGameFile(opt_.levelFile) : startGame(opt_.level);
                    if (!ok) {
                        std::fprintf(stderr, "geometry_dash: %s\n", error_.c_str());
                        changeState(AppState::Quit);
                        frame_ = -2;  // marker: failed start
                    }
                } else {
                    changeState(AppState::MainMenu);
                }
            }
            break;
        case AppState::MainMenu:
        case AppState::LevelSelect: updateMenus(in); break;
        case AppState::Playing: {
            if (!game_) break;
            game_->feedInput(in.held);
            if (in.checkpoint) game_->placeCheckpoint();
            if (in.removeCheckpoint) game_->removeCheckpoint();
            if (in.restart) game_->restart();
            if (in.togglePractice) {
                game_->setPractice(!game_->practiceMode());
                practiceSelected_ = game_->practiceMode();
            }
            if (in.pause || pendingPause_) {
                pendingPause_ = false;
                game_->setPaused(true);
                changeState(AppState::Paused);
            }
            break;
        }
        case AppState::Paused: updatePause(in); break;
        case AppState::Dead: updateDead(in); break;
        case AppState::Complete: updateComplete(in); break;
        case AppState::Quit: break;
    }
}

void App::drawFrame(double alpha) {
    Gfx& g = *gfx_;
    // Sharp text: tell the text helper how many output pixels one logical pixel covers.
    SDL_FRect pres{};
    if (SDL_GetRenderLogicalPresentationRect(renderer_, &pres) && pres.w > 0)
        text_.setPixelScale(pres.w / static_cast<float>(kWindowW));

    g.clear(rgba(0, 0, 0));
    DrawOptions dopt;
    dopt.hitboxes = opt_.hitboxes;
    const double menuTime = static_cast<double>(frame_) / 60.0;
    switch (state_) {
        case AppState::MainMenu: menu::drawMainMenu(g, text_, menuIndex_, menuTime); break;
        case AppState::LevelSelect:
            menu::drawLevelSelect(g, text_, levels_, progress_, levelIndex_, practiceSelected_, menuTime, toast_);
            break;
        case AppState::Playing:
        case AppState::Paused:
        case AppState::Dead:
        case AppState::Complete:
            if (game_) {
                drawGame(g, text_, *game_, alpha, dopt);
                if (state_ == AppState::Paused) menu::drawPause(g, text_, *game_, pauseIndex_);
                if (state_ == AppState::Complete)
                    menu::drawComplete(g, text_, *game_, progress_.get(game_->level().id), completeIndex_);
            }
            break;
        case AppState::Boot:
        case AppState::Quit: break;
    }
    g.flush();
}

bool App::saveScreenshot(const std::string& path) {
    SDL_Surface* surf = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surf) {
        std::fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        return false;
    }
    const bool ok = SDL_SaveBMP(surf, path.c_str());
    if (!ok) std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
    SDL_DestroySurface(surf);
    return ok;
}

int App::run() {
    if (!init()) return 1;

    directStart_ = opt_.level > 0 || !opt_.levelFile.empty();
    const bool smoke = opt_.smokeFrames >= 0;
    const bool shot = !opt_.screenshotFile.empty();
    const bool sequence = opt_.frameEvery > 0;
    const bool deterministic = smoke || shot || sequence;  // fixed dt, no real-time waits

    // Progress file: --save wins. Headless/scripted runs without --save keep progress in memory only,
    // so tests never touch the player's real save.
    std::string savePath = opt_.savePath;
    if (savePath.empty() && !deterministic) {
        if (char* pref = SDL_GetPrefPath("big-vibecode", "GeometryDashRemake")) {
            savePath = std::string(pref) + "progress.json";
            SDL_free(pref);
        }
    }
    progress_.setPath(savePath);
    progress_.load();
    const long lastFrame = shot ? opt_.screenshotFrame : (smoke ? opt_.smokeFrames - 1 : -1);
    if (sequence) {
        std::error_code ec;
        std::filesystem::create_directories(opt_.screenshotDir, ec);
        if (ec) {
            std::fprintf(stderr, "cannot create %s: %s\n", opt_.screenshotDir.c_str(), ec.message().c_str());
            return 1;
        }
    }

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    int exitCode = 0;
    int shotsTaken = 0;

    while (state_ != AppState::Quit) {
        double dt = 1.0 / 60.0;
        if (!deterministic) {
            const Uint64 now = SDL_GetPerformanceCounter();
            dt = static_cast<double>(now - prev) / freq;
            prev = now;
        }

        InputState in;
        pollInput(in);
        if (deterministic) scriptedInput(in);
        if (in.quit) {
            changeState(AppState::Quit);
            break;
        }

        update(in);
        if (frame_ == -2) {  // direct start failed
            exitCode = 1;
            break;
        }

        // Fixed-step ticks. Paused and menu states do not advance the game.
        ticksThisFrame_ = 0;
        double alpha = 0.0;
        if (game_ && (state_ == AppState::Playing || state_ == AppState::Dead || state_ == AppState::Complete)) {
            const int ticks = timestep_.advance(dt);
            for (int i = 0; i < ticks; ++i) game_->tick();
            game_->endFrame(ticks);
            ticksThisFrame_ = ticks;
            alpha = timestep_.alpha();
            game_->frame(dt, alpha);
            if (state_ == AppState::Playing && game_->dying()) changeState(AppState::Dead);
            else if (state_ == AppState::Dead && !game_->dying()) changeState(AppState::Playing);
            if (state_ == AppState::Playing && game_->showComplete()) changeState(AppState::Complete);
        } else if (game_ && state_ == AppState::Paused) {
            alpha = timestep_.alpha();
        }

        drawFrame(alpha);

        // Capture before present (some backends clear the frame on present).
        if (shot && frame_ == lastFrame) {
            if (!saveScreenshot(opt_.screenshotFile)) exitCode = 1;
            break;
        }
        if (sequence && state_ == AppState::Complete) break;  // level done: no more shots
        if (sequence && frame_ % opt_.frameEvery == 0) {
            char name[64];
            std::snprintf(name, sizeof name, "/frame_%05ld.bmp", frame_);
            if (!saveScreenshot(opt_.screenshotDir + name)) exitCode = 1;
            if (++shotsTaken >= opt_.maxShots) {
                std::fprintf(stderr, "warning: --frame-every stopped at the shot cap (%d); raise it with --max-shots N (max %d)\n",
                             opt_.maxShots, kMaxSequenceShotsLimit);
                break;
            }
        }
        SDL_RenderPresent(renderer_);

        if (smoke && frame_ >= lastFrame) break;
        ++frame_;
    }
    if (smoke && exitCode == 0) {
        std::printf("smoke ok: frames=%ld state=%s audio=%s", frame_ + 1, stateName(state_),
                    audio_.enabled() ? "on" : "off");
        if (game_)
            std::printf(" level=%d percent=%d attempts=%d won=%d", game_->level().id, game_->percent(),
                        game_->attempts(), game_->won() ? 1 : 0);
        std::printf("\n");
    }
    return exitCode;
}

}  // namespace gd
