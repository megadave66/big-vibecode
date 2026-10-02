#include "App.hpp"

#include "TableScene.hpp"

#include <filesystem>

namespace bac::app {
namespace {

namespace fs = std::filesystem;

std::string findAssetDir() {
  if (const char* base = SDL_GetBasePath()) {
    fs::path p = fs::path(base) / "assets";
    std::error_code ec;
    if (fs::is_directory(p, ec)) return p.string();
  }
#ifdef BAC_SOURCE_ASSETS_DIR
  return BAC_SOURCE_ASSETS_DIR;
#else
  return "assets";
#endif
}

TTF_Font* openFont(const std::string& assetDir, const char* file, float size) {
  std::string path = assetDir + "/fonts/" + file;
  TTF_Font* f = TTF_OpenFont(path.c_str(), size);
  if (!f) SDL_Log("warning: font %s not loaded: %s", path.c_str(), SDL_GetError());
  return f;
}

}  // namespace

std::unique_ptr<App> App::create(const AppOptions& opts) {
  std::unique_ptr<App> app(new App());
  app->opts_ = opts;
  app->ctx_.opts = &app->opts_;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("fatal: SDL_Init failed: %s", SDL_GetError());
    return nullptr;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_Log("warning: audio unavailable: %s", SDL_GetError());
  }

  app->window_ = SDL_CreateWindow("Baccarat", kLogicalWidth, kLogicalHeight,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!app->window_) {
    SDL_Log("fatal: SDL_CreateWindow failed: %s", SDL_GetError());
    return nullptr;
  }
  SDL_Renderer* r = SDL_CreateRenderer(app->window_, nullptr);
  if (!r) r = SDL_CreateRenderer(app->window_, SDL_SOFTWARE_RENDERER);
  if (!r) {
    SDL_Log("fatal: SDL_CreateRenderer failed: %s", SDL_GetError());
    return nullptr;
  }
  app->ctx_.renderer = r;
  SDL_SetRenderLogicalPresentation(r, kLogicalWidth, kLogicalHeight,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  // Real-time play: vsync. Scripted/frame-limited runs never wait on it.
  if (!opts.frames) SDL_SetRenderVSync(r, 1);

  app->ctx_.assetDir = findAssetDir();

  if (TTF_Init()) {
    app->ttfReady_ = true;
    Fonts& f = app->ctx_.fonts;
    f.small = openFont(app->ctx_.assetDir, "Lato-Regular.ttf", 16);
    f.body = openFont(app->ctx_.assetDir, "Lato-Regular.ttf", 22);
    f.bold = openFont(app->ctx_.assetDir, "Lato-Bold.ttf", 28);
    f.title = openFont(app->ctx_.assetDir, "Lato-Bold.ttf", 52);
  } else {
    SDL_Log("warning: TTF_Init failed, no text: %s", SDL_GetError());
  }

  if (MIX_Init()) {
    app->mixReady_ = true;
    app->ctx_.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!app->ctx_.mixer) SDL_Log("warning: no audio device, running silent: %s", SDL_GetError());
  } else {
    SDL_Log("warning: MIX_Init failed, running silent: %s", SDL_GetError());
  }

  app->scene_ = std::make_unique<TableScene>(app->ctx_);
  return app;
}

App::~App() {
  scene_.reset();  // scenes free their textures/audio before the renderer/mixer go
  if (ctx_.mixer) MIX_DestroyMixer(ctx_.mixer);
  if (mixReady_) MIX_Quit();
  for (TTF_Font* f : {ctx_.fonts.small, ctx_.fonts.body, ctx_.fonts.bold, ctx_.fonts.title}) {
    if (f) TTF_CloseFont(f);
  }
  if (ttfReady_) TTF_Quit();
  if (ctx_.renderer) SDL_DestroyRenderer(ctx_.renderer);
  if (window_) SDL_DestroyWindow(window_);
  SDL_Quit();
}

void App::frame(bool lastFrame) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
      quitRequested_ = true;
      continue;
    }
    // Map window coordinates to the 1280x720 logical space.
    SDL_ConvertEventToRenderCoordinates(ctx_.renderer, &e);
    scene_->handleEvent(e);
  }
  scene_->update(kFixedDt);
  scene_->render(ctx_.renderer);
  if (lastFrame && opts_.screenshot) {
    if (!saveScreenshot(*opts_.screenshot)) screenshotFailed_ = true;
  }
  SDL_RenderPresent(ctx_.renderer);
}

bool App::saveScreenshot(const std::string& path) {
  SDL_Surface* s = SDL_RenderReadPixels(ctx_.renderer, nullptr);
  if (!s) {
    SDL_Log("error: SDL_RenderReadPixels failed: %s", SDL_GetError());
    return false;
  }
  bool ok = SDL_SaveBMP(s, path.c_str());
  if (!ok) SDL_Log("error: SDL_SaveBMP(%s) failed: %s", path.c_str(), SDL_GetError());
  SDL_DestroySurface(s);
  return ok;
}

int App::run() {
  if (opts_.frames) {
    // Deterministic mode: exactly N fixed steps, no wall clock.
    const int n = *opts_.frames;
    for (int i = 0; i < n && !quitRequested_ && !scene_->wantsQuit(); ++i) {
      bool last = (i == n - 1);
      frame(last);
    }
    if (screenshotFailed_) return 3;
    return scene_->exitCode();
  }
  // Interactive mode: fixed-step updates driven by the wall clock.
  Uint64 prev = SDL_GetTicksNS();
  double acc = 0.0;
  while (!quitRequested_ && !scene_->wantsQuit()) {
    Uint64 now = SDL_GetTicksNS();
    acc += double(now - prev) / 1e9;
    prev = now;
    if (acc > 0.25) acc = 0.25;  // avoid a spiral after a stall
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        quitRequested_ = true;
        continue;
      }
      SDL_ConvertEventToRenderCoordinates(ctx_.renderer, &e);
      scene_->handleEvent(e);
    }
    while (acc >= kFixedDt) {
      scene_->update(kFixedDt);
      acc -= kFixedDt;
    }
    scene_->render(ctx_.renderer);
    SDL_RenderPresent(ctx_.renderer);
    SDL_Delay(1);
  }
  return scene_->exitCode();
}

}  // namespace bac::app
