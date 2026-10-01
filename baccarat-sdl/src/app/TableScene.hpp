#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Audio.hpp"
#include "DealAnimator.hpp"
#include "Gfx.hpp"
#include "Scene.hpp"
#include "baccarat/game/script.hpp"
#include "baccarat/game/session.hpp"

namespace bac::app {

struct AppContext;

// The baccarat table: betting, dealing animation, result, history, scripted input.
class TableScene : public Scene {
 public:
  explicit TableScene(AppContext& ctx);
  ~TableScene() override;
  void handleEvent(const SDL_Event& event) override;
  void update(double dt) override;
  void render(SDL_Renderer* renderer) override;
  bool wantsQuit() const override { return quit_; }
  int exitCode() const override { return exitCode_; }

 private:
  // Actions shared by mouse, keyboard and script.
  void selectChip(int index);
  void placeAt(bac::rules::BetSpot spot);
  void removeAt(bac::rules::BetSpot spot);
  void doClear();
  void doRebet();
  void doDeal();
  void doNewSession();
  void toggleMute();
  void toast(const std::string& text);

  // Flow.
  void stepDealing(double dt);
  void doCollect();
  void skipStage();

  // Script.
  void stepScript();
  void scriptFail(const std::string& msg);

  // Drawing.
  void drawFelt(SDL_Renderer* r);
  void drawTopBar();
  void drawHands();
  void drawHandLabel(int side, const std::string& total, bool winner);
  void drawCard(const layout::Rect& rc, double scaleX, bool faceUp,
                const bac::rules::Card* card, uint8_t alpha = 255);
  void drawShoe();
  void drawBanner();
  void drawSpots();
  void drawChipLabel(TTF_Font* f, std::int64_t cents, float x, float y);
  void drawChipSprite(std::int64_t cents, float cx, float cy, float size);
  void drawTray();
  void drawButton(const layout::Rect& rc, const char* label, bool enabled, SDL_Color base,
                  bool hover);
  void drawHistory();
  void drawToast();
  void drawBrokeOverlay();
  void drawText(TTF_Font* f, const std::string& s, SDL_Color c, float x, float y,
                gfx::Align a = gfx::Align::Left, uint8_t alpha = 255);
  void drawShadowText(TTF_Font* f, const std::string& s, SDL_Color c, float x, float y,
                      gfx::Align a = gfx::Align::Center);
  bool hover(const layout::Rect& rc) const { return rc.contains(mouseX_, mouseY_); }

  AppContext& ctx_;
  SDL_Renderer* r_;
  bac::game::Session session_;
  SfxBank sfx_;
  gfx::TextCache text_;
  gfx::ImageCache images_;

  // Round presentation.
  bool shown_ = false;  // cards/banner of the last round are on screen
  std::unique_ptr<DealAnimator> anim_;
  double animT_ = 0, animPrev_ = -1;
  double stageT_ = 0;  // time in Resolution / Payout

  // UI state.
  float mouseX_ = -1, mouseY_ = -1;
  std::string toastText_;
  double toastT_ = 0;

  // Script / lifecycle.
  std::vector<bac::game::ScriptCmd> script_;
  size_t scriptIdx_ = 0;
  int waitFrames_ = 0;
  bool scriptActive_ = false;
  std::vector<std::string> shotPaths_;
  bool hadPrevBets_ = false;
  bool quit_ = false;
  int exitCode_ = 0;
};

}  // namespace bac::app
