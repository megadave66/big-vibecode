#include "TableScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "App.hpp"
#include "Format.hpp"
#include "Layout.hpp"

namespace bac::app {
namespace {

using bac::game::Phase;
using bac::rules::BetSpot;
using bac::rules::Outcome;
using bac::rules::Side;
using gfx::Align;

constexpr double kCollectDelay = 1.2;
constexpr double kNextDelay = 1.0;
constexpr double kToastSeconds = 2.4;

const SDL_Color kPlayerCol{70, 140, 235, 255};
const SDL_Color kBankerCol{225, 75, 75, 255};
const SDL_Color kTieCol{70, 185, 105, 255};
const SDL_Color kGold{236, 205, 120, 255};
const SDL_Color kWhite{245, 245, 240, 255};
const SDL_Color kDim{170, 190, 175, 255};
const SDL_Color kInk{12, 38, 24, 255};

SDL_Color spotColor(int spot) {
  return spot == 0 ? kPlayerCol : spot == 1 ? kBankerCol : kTieCol;
}
const char* spotName(int spot) { return spot == 0 ? "PLAYER" : spot == 1 ? "BANKER" : "TIE"; }
SDL_Color outcomeColor(Outcome o) {
  return o == Outcome::Player ? kPlayerCol : o == Outcome::Banker ? kBankerCol : kTieCol;
}
BetSpot spotFromIndex(int i) { return bac::rules::kAllBetSpots[static_cast<size_t>(i)]; }

std::uint64_t pickSeed(const AppOptions* o) {
  if (o && o->seed) return *o->seed;
  return SDL_GetTicksNS() ^ (SDL_GetPerformanceCounter() * 0x9E3779B97F4A7C15ull);
}

std::string chipPath(const std::string& dir, std::int64_t cents) {
  return dir + "/chips/chip_" + std::to_string(cents / 100) + ".png";
}

}  // namespace

TableScene::TableScene(AppContext& ctx)
    : ctx_(ctx),
      r_(ctx.renderer),
      session_(pickSeed(ctx.opts)),
      sfx_(ctx.mixer, ctx.assetDir),
      text_(ctx.renderer),
      images_(ctx.renderer) {
  if (ctx_.opts && ctx_.opts->script) {
    try {
      script_ = bac::game::parseScript(*ctx_.opts->script);
      scriptActive_ = true;
      SDL_Log("script: %zu commands", script_.size());
    } catch (const std::exception& e) {
      SDL_Log("SCRIPT PARSE ERROR: %s", e.what());
      exitCode_ = 2;
      quit_ = true;
    }
  }
}

TableScene::~TableScene() = default;

// ---------------------------------------------------------------- actions

void TableScene::toast(const std::string& t) {
  toastText_ = t;
  toastT_ = kToastSeconds;
  SDL_Log("toast: %s", t.c_str());
}

void TableScene::selectChip(int index) {
  if (index < 0 || index >= layout::kChipCount) return;
  session_.selectChip(bac::rules::kChipValuesCents[static_cast<size_t>(index)]);
}

void TableScene::placeAt(BetSpot spot) {
  if (session_.phase() != Phase::Betting || session_.isBroke()) return;
  const auto res = session_.placeBet(spot);
  using S = bac::game::PlaceResult::Status;
  switch (res.status) {
    case S::Placed:
      sfx_.play(Sfx::ChipPlace);
      shown_ = false;
      break;
    case S::AllIn:
      sfx_.play(Sfx::ChipPlace);
      shown_ = false;
      toast("All-in: " + formatMoney(res.placedCents) + " placed");
      break;
    case S::InsufficientFunds:
      toast("Not enough funds");
      break;
    case S::WrongPhase:
      break;
  }
}

void TableScene::removeAt(BetSpot spot) {
  if (session_.phase() != Phase::Betting) return;
  if (session_.removeBet(spot)) sfx_.play(Sfx::ChipRemove);
}

void TableScene::doClear() {
  if (session_.phase() != Phase::Betting) return;
  if (session_.clearBets()) sfx_.play(Sfx::ChipRemove);
}

void TableScene::doRebet() {
  if (session_.phase() != Phase::Betting || session_.isBroke()) return;
  using R = bac::game::RebetStatus;
  switch (session_.rebet()) {
    case R::Placed:
      sfx_.play(Sfx::ChipPlace);
      shown_ = false;
      break;
    case R::Partial:
      sfx_.play(Sfx::ChipPlace);
      shown_ = false;
      toast("Partial rebet: not enough funds for every chip");
      break;
    case R::NothingToRebet:
      toast("Nothing to rebet");
      break;
    case R::TableNotEmpty:
      toast("Clear the table first");
      break;
    case R::WrongPhase:
      break;
  }
}

void TableScene::doDeal() {
  if (session_.phase() != Phase::Betting || session_.isBroke()) return;
  if (!session_.canDeal()) {
    toast("Place a bet first");
    return;
  }
  const bool hadBets = session_.totalOnTable() > 0;
  if (!session_.deal()) return;
  if (hadBets) hadPrevBets_ = true;
  shown_ = true;
  anim_ = std::make_unique<DealAnimator>(static_cast<int>(session_.lastRound().deals.size()));
  animT_ = 0;
  animPrev_ = -1;
  if (session_.reshuffledThisRound()) toast("Shuffling new shoe");
}

void TableScene::doNewSession() {
  session_.resetSession();
  hadPrevBets_ = false;
  shown_ = false;
  anim_.reset();
  toast("New session: " + formatMoneyShort(session_.bankroll()));
}

void TableScene::toggleMute() {
  sfx_.setMuted(!sfx_.muted());
  SDL_Log("sound %s", sfx_.muted() ? "muted" : "on");
}

// ------------------------------------------------------------------- flow

void TableScene::stepDealing(double dt) {
  if (!anim_) return;
  animT_ += dt;
  for (int i = 0; i < anim_->count(); ++i) {
    const double s = anim_->slideStart(i), f = anim_->flipStart(i);
    if (s > animPrev_ && s <= animT_) sfx_.play(Sfx::CardSlide);
    if (f > animPrev_ && f <= animT_) sfx_.play(Sfx::CardFlip);
  }
  animPrev_ = animT_;
  if (anim_->finished(animT_)) {
    session_.finishDealing();
    stageT_ = 0;
  }
}

void TableScene::doCollect() {
  session_.collect();
  const auto net = session_.lastNetCents();
  if (net > 0) sfx_.play(Sfx::Win);
  else if (net < 0) sfx_.play(Sfx::Lose);
  stageT_ = 0;
}

void TableScene::skipStage() {
  switch (session_.phase()) {
    case Phase::Dealing:
      if (anim_) {
        animT_ = anim_->totalDuration();
        animPrev_ = animT_;
        session_.finishDealing();
        stageT_ = 0;
      }
      break;
    case Phase::Resolution:
      doCollect();
      break;
    case Phase::Payout:
      session_.nextRound();
      stageT_ = 0;
      break;
    case Phase::Betting:
      break;
  }
}

void TableScene::update(double dt) {
  stepScript();
  if (toastT_ > 0) toastT_ -= dt;
  switch (session_.phase()) {
    case Phase::Dealing:
      stepDealing(dt);
      break;
    case Phase::Resolution:
      stageT_ += dt;
      if (stageT_ >= kCollectDelay) doCollect();
      break;
    case Phase::Payout:
      stageT_ += dt;
      if (stageT_ >= kNextDelay) {
        session_.nextRound();
        stageT_ = 0;
      }
      break;
    case Phase::Betting:
      break;
  }
}

// ----------------------------------------------------------------- script

void TableScene::scriptFail(const std::string& msg) {
  SDL_Log("SCRIPT ASSERT FAILED: %s", msg.c_str());
  exitCode_ = 4;
  scriptIdx_ = script_.size();
}

void TableScene::stepScript() {
  if (!scriptActive_) return;
  if (waitFrames_ > 0) {
    --waitFrames_;
    return;
  }
  if (scriptIdx_ >= script_.size()) {
    scriptActive_ = false;
    if (!(ctx_.opts && ctx_.opts->frames)) quit_ = true;
    SDL_Log("script: finished (exit code %d)", exitCode_);
    return;
  }
  using bac::game::ScriptOp;
  const auto& c = script_[scriptIdx_++];
  switch (c.op) {
    case ScriptOp::Chip: {
      SDL_Log("script: chip %lld", static_cast<long long>(c.value));
      session_.selectChip(c.value);
      break;
    }
    case ScriptOp::Bet:
      SDL_Log("script: bet %s", spotName(static_cast<int>(c.spot)));
      placeAt(c.spot);
      break;
    case ScriptOp::Remove:
      SDL_Log("script: remove %s", spotName(static_cast<int>(c.spot)));
      removeAt(c.spot);
      break;
    case ScriptOp::Clear:
      SDL_Log("script: clear");
      doClear();
      break;
    case ScriptOp::Rebet:
      SDL_Log("script: rebet");
      doRebet();
      break;
    case ScriptOp::Deal:
      SDL_Log("script: deal");
      doDeal();
      break;
    case ScriptOp::Wait:
      SDL_Log("script: wait %lld frames", static_cast<long long>(c.value));
      waitFrames_ = static_cast<int>(c.value);
      break;
    case ScriptOp::Shot:
      SDL_Log("script: shot %s", c.text.c_str());
      shotPaths_.push_back(c.text);
      break;
    case ScriptOp::Next:
      SDL_Log("script: next (fast-forward stage)");
      skipStage();
      break;
    case ScriptOp::AssertBankroll:
      SDL_Log("script: assert-bankroll %lld", static_cast<long long>(c.value));
      if (session_.bankroll() != c.value)
        scriptFail("bankroll is " + std::to_string(session_.bankroll()) + ", expected " +
                   std::to_string(c.value));
      break;
    case ScriptOp::AssertPhase: {
      static const char* names[] = {"betting", "dealing", "resolution", "payout"};
      SDL_Log("script: assert-phase %s", names[static_cast<int>(c.phase)]);
      if (session_.phase() != c.phase)
        scriptFail(std::string("phase is ") + names[static_cast<int>(session_.phase())] +
                   ", expected " + names[static_cast<int>(c.phase)]);
      break;
    }
    case ScriptOp::Quit:
      SDL_Log("script: quit");
      quit_ = true;
      break;
  }
}

// ------------------------------------------------------------------ input

void TableScene::handleEvent(const SDL_Event& e) {
  using layout::HitKind;
  switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
      mouseX_ = e.motion.x;
      mouseY_ = e.motion.y;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      mouseX_ = e.button.x;
      mouseY_ = e.button.y;
      const layout::Hit h = layout::hitTest(mouseX_, mouseY_, session_.isBroke());
      if (e.button.button == SDL_BUTTON_LEFT) {
        switch (h.kind) {
          case HitKind::Spot: placeAt(spotFromIndex(h.index)); break;
          case HitKind::SpotRemove: removeAt(spotFromIndex(h.index)); break;
          case HitKind::Chip: selectChip(h.index); break;
          case HitKind::Deal: doDeal(); break;
          case HitKind::Clear: doClear(); break;
          case HitKind::Rebet: doRebet(); break;
          case HitKind::NewSession: doNewSession(); break;
          case HitKind::Mute: toggleMute(); break;
          case HitKind::None: break;
        }
      } else if (e.button.button == SDL_BUTTON_RIGHT) {
        if (h.kind == HitKind::Spot || h.kind == HitKind::SpotRemove)
          removeAt(spotFromIndex(h.index));
      }
      break;
    }
    case SDL_EVENT_KEY_DOWN: {
      if (e.key.repeat) break;
      const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
      const SDL_Keycode k = e.key.key;
      if (k == SDLK_ESCAPE) {
        quit_ = true;
      } else if (k == SDLK_M) {
        toggleMute();
      } else if (session_.isBroke()) {
        if (k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER) doNewSession();
      } else if (k >= SDLK_1 && k <= SDLK_4) {
        selectChip(int(k - SDLK_1));
      } else if (k == SDLK_P || k == SDLK_B || k == SDLK_T) {
        const BetSpot s = k == SDLK_P ? BetSpot::Player : k == SDLK_B ? BetSpot::Banker : BetSpot::Tie;
        if (shift) removeAt(s);
        else placeAt(s);
      } else if (k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        doDeal();
      } else if (k == SDLK_BACKSPACE) {
        doClear();
      } else if (k == SDLK_R) {
        doRebet();
      }
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------- drawing

void TableScene::drawText(TTF_Font* f, const std::string& s, SDL_Color c, float x, float y,
                          Align a, uint8_t alpha) {
  text_.draw(f, s, c, x, y, a, alpha);
}

void TableScene::drawShadowText(TTF_Font* f, const std::string& s, SDL_Color c, float x, float y,
                                Align a) {
  text_.draw(f, s, SDL_Color{0, 0, 0, 255}, x + 1.5f, y + 1.5f, a, 160);
  text_.draw(f, s, c, x, y, a);
}

void TableScene::drawFelt(SDL_Renderer* r) {
  gfx::gradientRect(r, {0, 0, layout::kWidth, layout::kHeight}, SDL_Color{16, 104, 62, 255},
                    SDL_Color{7, 62, 38, 255});
  // Vignette: dark translucent frames, strongest at the edge.
  for (int i = 0; i < 36; ++i) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(70 - i * 2));
    SDL_FRect f{float(i), float(i), layout::kWidth - 2.0f * i, layout::kHeight - 2.0f * i};
    SDL_RenderRect(r, &f);
  }
}

void TableScene::drawTopBar() {
  gfx::fillRoundRect(r_, {0, 0, layout::kWidth, layout::kBarHeight}, 0, SDL_Color{0, 0, 0, 255},
                     120);
  const auto& F = ctx_.fonts;
  const float y = 24;
  drawText(F.bold, "BACCARAT", kGold, 24, y);
  float x = 210;
  auto pair = [&](const char* label, const std::string& value) {
    drawText(F.small, label, kDim, x, y + 1);
    x += text_.width(F.small, label) + 8;
    drawText(F.bold, value, kWhite, x, y);
    x += text_.width(F.bold, value) + 34;
  };
  pair("Bankroll", formatMoney(session_.bankroll()));
  pair("On table", formatMoney(session_.totalOnTable()));
  std::size_t shoe = session_.shoe().remaining();
  if (session_.phase() == Phase::Dealing && anim_) {
    int pending = 0;
    for (int i = 0; i < anim_->count(); ++i)
      if (anim_->slideStart(i) > animT_) ++pending;
    shoe += static_cast<std::size_t>(pending);
  }
  pair("Shoe", std::to_string(shoe) + " cards");
  const auto m = layout::muteRect();
  const bool muted = sfx_.muted();
  gfx::fillRoundRect(r_, gfx::toF(m), 8, muted ? SDL_Color{150, 50, 50, 255} : SDL_Color{255, 255, 255, 255},
                     hover(m) ? 70 : 36);
  drawText(F.body, muted ? "Sound: MUTED  [M]" : "Sound: ON  [M]", muted ? SDL_Color{255, 190, 190, 255} : kWhite,
           m.cx(), m.cy(), Align::Center);
}

void TableScene::drawCard(const layout::Rect& rc, double scaleX, bool faceUp,
                          const bac::rules::Card* card, uint8_t alpha) {
  const float w = rc.w * float(scaleX);
  SDL_FRect dst{rc.cx() - w / 2, rc.y, w, rc.h};
  if (w < 1) return;
  // Shadow.
  gfx::fillRoundRect(r_, {dst.x + 3, dst.y + 5, dst.w, dst.h}, 7, SDL_Color{0, 0, 0, 255}, 70);
  std::string path = ctx_.assetDir + "/cards/";
  path += (faceUp && card) ? bac::rules::toString(*card) + ".png" : "back.png";
  if (SDL_Texture* t = images_.get(path)) {
    SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderTexture(r_, t, nullptr, &dst);
    return;
  }
  // Fallback: plain rectangle.
  if (faceUp && card) {
    gfx::fillRoundRect(r_, dst, 7, SDL_Color{250, 250, 245, 255});
    if (scaleX > 0.6)
      drawText(ctx_.fonts.bold, bac::rules::toString(*card), kInk, dst.x + dst.w / 2,
               dst.y + dst.h / 2, Align::Center);
  } else {
    gfx::fillRoundRect(r_, dst, 7, SDL_Color{40, 60, 150, 255});
    gfx::strokeRoundRect(r_, dst, 7, 3, SDL_Color{240, 240, 240, 255});
  }
}

void TableScene::drawShoe() {
  const auto s = layout::shoeRect();
  for (int i = 2; i >= 0; --i) {
    layout::Rect rc{s.x + i * 3.0f, s.y - i * 3.0f, s.w, s.h};
    drawCard(rc, 1.0, false, nullptr);
  }
  drawText(ctx_.fonts.small, "SHOE", kDim, s.cx(), s.bottom() + 20, Align::Center);
}

void TableScene::drawHandLabel(int side, const std::string& total, bool winner) {
  const auto& F = ctx_.fonts;
  const float cx = layout::handCenterX(side);
  const SDL_Color col = side == 0 ? kPlayerCol : kBankerCol;
  const float y = 76;
  drawShadowText(F.bold, side == 0 ? "PLAYER" : "BANKER", kWhite, cx - 20, y);
  gfx::fillRoundRect(r_, {cx - 124, y + 22, 248, 4}, 2, col);  // accent bar
  if (!total.empty()) {
    const layout::Rect b{cx + 50, y - 19, 52, 38};
    gfx::fillRoundRect(r_, gfx::toF(b), 10, SDL_Color{0, 0, 0, 255}, 130);
    gfx::strokeRoundRect(r_, gfx::toF(b), 10, winner ? 3 : 2, winner ? kGold : col);
    drawText(F.bold, total, winner ? kGold : kWhite, b.cx(), b.cy(), Align::Center);
  }
}

void TableScene::drawHands() {
  const auto& rr = session_.lastRound();
  const bool animating = session_.phase() == Phase::Dealing && anim_;
  const bool resultKnown = shown_ && !animating;
  int revealed[2] = {0, 0};
  // Empty slots when nothing is on the table.
  if (!shown_) {
    for (int side = 0; side < 2; ++side)
      for (int i = 0; i < 2; ++i) {
        const auto rc = layout::cardRect(side, i);
        gfx::strokeRoundRect(r_, gfx::toF(rc), 9, 2, kWhite, 40);
      }
  }
  if (shown_) {
    for (std::size_t i = 0; i < rr.deals.size(); ++i) {
      const auto& d = rr.deals[i];
      const int side = d.side == Side::Player ? 0 : 1;
      const layout::Rect slot = layout::cardRect(side, d.index);
      CardAnim a;
      if (animating) {
        a = anim_->at(static_cast<int>(i), animT_);
      } else {
        a.visible = true;
        a.stage = CardAnim::Stage::Done;
        a.slide = 1;
        a.faceUp = true;
        a.scaleX = 1;
      }
      if (!a.visible) continue;
      const layout::Rect from = layout::shoeRect();
      const float k = float(a.slide);
      layout::Rect cur{from.x + (slot.x - from.x) * k, from.y + (slot.y - from.y) * k,
                       from.w + (slot.w - from.w) * k, from.h + (slot.h - from.h) * k};
      drawCard(cur, a.scaleX, a.faceUp, &d.card);
      if (a.faceUp && cur.w * float(a.scaleX) >= 1.0f) ++revealed[side];
    }
  }
  for (int side = 0; side < 2; ++side) {
    std::string total;
    if (revealed[side] > 0) {
      const auto& cards = side == 0 ? rr.player : rr.banker;
      total = std::to_string(bac::rules::handTotal(
          std::span<const bac::rules::Card>(cards.data(), std::size_t(revealed[side]))));
    }
    const bool winner = resultKnown && ((side == 0 && rr.outcome == Outcome::Player) ||
                                        (side == 1 && rr.outcome == Outcome::Banker));
    drawHandLabel(side, total, winner);
  }
}

void TableScene::drawBanner() {
  const Phase ph = session_.phase();
  if (!shown_ || ph == Phase::Dealing) return;
  const auto& rr = session_.lastRound();
  const auto& st = session_.lastSettlement();
  const auto b = layout::bannerRect();
  const SDL_Color oc = outcomeColor(rr.outcome);
  gfx::fillRoundRect(r_, gfx::toF(b), 14, SDL_Color{0, 0, 0, 255}, 150);
  gfx::strokeRoundRect(r_, gfx::toF(b), 14, 3, oc);
  const auto& F = ctx_.fonts;
  drawShadowText(F.bold, bannerText(rr.outcome, rr.playerTotal, rr.bankerTotal, rr.natural), kWhite,
                 b.cx(), b.y + 24);
  std::int64_t stake = 0;
  for (const auto& s : st.spots) stake += s.stake;
  const std::string net = netText(stake, st.netCents, st.commissionCents);
  const SDL_Color nc = st.netCents > 0 ? SDL_Color{130, 240, 150, 255}
                       : st.netCents < 0 ? SDL_Color{255, 140, 140, 255}
                                         : kDim;
  drawText(F.body, net, stake > 0 ? nc : kDim, b.cx(), b.y + 52, Align::Center);
}

// Chip denomination text; the white $1 chip gets dark text with a light outline.
void TableScene::drawChipLabel(TTF_Font* f, std::int64_t cents, float x, float y) {
  const std::string t = formatMoneyShort(cents);
  if (cents < 500) {
    const SDL_Color light{255, 255, 255, 255};
    for (float dx : {-1.5f, 0.0f, 1.5f})
      for (float dy : {-1.5f, 0.0f, 1.5f})
        if (dx != 0 || dy != 0) text_.draw(f, t, light, x + dx, y + dy, Align::Center);
    text_.draw(f, t, SDL_Color{10, 25, 60, 255}, x, y, Align::Center);
  } else {
    drawShadowText(f, t, kWhite, x, y, Align::Center);
  }
}

void TableScene::drawChipSprite(std::int64_t cents, float cx, float cy, float size) {
  SDL_FRect dst{cx - size / 2, cy - size / 2, size, size};
  // Odd amounts (all-in) borrow the largest sprite not above the amount.
  std::int64_t sprite = bac::rules::kChipValuesCents[0];
  for (std::int64_t v : bac::rules::kChipValuesCents)
    if (v <= cents) sprite = v;
  if (SDL_Texture* t = images_.get(chipPath(ctx_.assetDir, sprite))) {
    SDL_RenderTexture(r_, t, nullptr, &dst);
  } else {
    gfx::fillCircle(r_, cx, cy, size / 2, SDL_Color{200, 200, 200, 255});
    gfx::strokeCircle(r_, cx, cy, size / 2, 3, SDL_Color{60, 60, 60, 255});
  }
}

void TableScene::drawSpots() {
  const auto& F = ctx_.fonts;
  const bool betting = session_.phase() == Phase::Betting && !session_.isBroke();
  const bool resultShown = shown_ && session_.phase() != Phase::Dealing;
  for (int i = 0; i < layout::kSpotCount; ++i) {
    const BetSpot spot = spotFromIndex(i);
    const auto rc = layout::spotRect(i);
    const SDL_Color col = spotColor(i);
    const auto& stack = session_.stack(spot);
    bool win = false;
    const auto& ss = session_.lastSettlement().at(spot);
    if (resultShown && ss.result == bac::rules::BetResult::Win) win = true;
    const bool hot = betting && hover(rc);

    gfx::fillRoundRect(r_, gfx::toF(rc), 22, SDL_Color{0, 0, 0, 255}, 60);
    if (hot) gfx::fillRoundRect(r_, gfx::toF(rc), 22, col, 40);
    if (win) {
      gfx::fillRoundRect(r_, gfx::toF(rc), 22, kGold, 55);
      gfx::strokeRoundRect(r_, {rc.x - 4, rc.y - 4, rc.w + 8, rc.h + 8}, 26, 3, kGold, 120);
    }
    gfx::strokeRoundRect(r_, gfx::toF(rc), 22, win ? 4 : 3, win ? kGold : col, hot || win ? 255 : 210);

    drawShadowText(F.bold, spotName(i), kWhite, rc.cx(), rc.y + 28);
    drawText(F.body, bac::rules::payoutLabel(spot), kGold, rc.cx(), rc.y + 58, Align::Center);
    const char* key = i == 0 ? "P" : i == 1 ? "B" : "T";
    drawText(F.small, key, kDim, rc.x + 18, rc.y + 24);

    // Chip stack (up to 8 sprites, offset upward).
    const std::size_t shown = std::min<std::size_t>(stack.size(), 5);
    const float baseY = rc.y + 118;
    for (std::size_t k = 0; k < shown; ++k) {
      const std::int64_t c = stack[stack.size() - shown + k];
      const float cy = baseY - float(k) * 6.0f;
      drawChipSprite(c, rc.cx(), cy, 54);
    }
    if (shown > 0) {
      const float topY = baseY - float(shown - 1) * 6.0f;
      const std::int64_t c = stack.back();
      drawChipLabel(F.small, c, rc.cx(), topY);
    }
    // Total or result under the stack.
    if (!stack.empty()) {
      drawText(F.bold, formatMoney(session_.bets().at(spot)), kWhite, rc.cx(), rc.y + 154,
               Align::Center);
    } else if (resultShown && ss.stake > 0) {
      using R = bac::rules::BetResult;
      std::string t = ss.result == R::Win ? formatSignedMoney(ss.netCents)
                      : ss.result == R::Push ? "PUSH"
                                             : formatSignedMoney(ss.netCents);
      SDL_Color tc = ss.result == R::Win ? SDL_Color{130, 240, 150, 255}
                     : ss.result == R::Push ? kDim
                                            : SDL_Color{255, 140, 140, 255};
      drawText(F.bold, t, tc, rc.cx(), rc.y + 118, Align::Center);
    }
    // Remove control.
    if (betting && !stack.empty()) {
      const auto m = layout::spotRemoveRect(i);
      const bool mh = hover(m);
      gfx::fillRoundRect(r_, gfx::toF(m), 8, mh ? SDL_Color{200, 70, 70, 255} : SDL_Color{0, 0, 0, 255},
                         mh ? 255 : 150);
      gfx::strokeRoundRect(r_, gfx::toF(m), 8, 2, kWhite, 200);
      drawText(F.bold, "-", kWhite, m.cx(), m.cy() - 1, Align::Center);
    }
  }
}

void TableScene::drawButton(const layout::Rect& rc, const char* label, bool enabled,
                            SDL_Color base, bool hov) {
  const SDL_Color fill = enabled ? base : SDL_Color{70, 85, 78, 255};
  SDL_FRect f = gfx::toF(rc);
  gfx::fillRoundRect(r_, {f.x, f.y + 3, f.w, f.h}, 14, SDL_Color{0, 0, 0, 255}, 90);
  gfx::fillRoundRect(r_, f, 14, fill);
  if (enabled && hov) gfx::fillRoundRect(r_, f, 14, SDL_Color{255, 255, 255, 255}, 40);
  gfx::strokeRoundRect(r_, f, 14, 2, enabled ? kGold : SDL_Color{110, 125, 118, 255}, 255);
  drawText(ctx_.fonts.bold, label, enabled ? kWhite : SDL_Color{150, 165, 158, 255}, rc.cx(),
           rc.cy(), Align::Center);
}

void TableScene::drawTray() {
  const bool betting = session_.phase() == Phase::Betting && !session_.isBroke();
  for (int i = 0; i < layout::kChipCount; ++i) {
    const std::int64_t cents = bac::rules::kChipValuesCents[static_cast<size_t>(i)];
    const auto rc = layout::chipRect(i);
    const bool sel = session_.selectedChip() == cents;
    const bool hov = betting && layout::chipHitRect(i).contains(mouseX_, mouseY_);
    const float lift = sel ? layout::kChipRaise : (hov ? 5.0f : 0.0f);
    const float cx = rc.cx(), cy = rc.cy() - lift;
    gfx::fillRoundRect(r_, {cx - 26, rc.bottom() - 4, 52, 8}, 4, SDL_Color{0, 0, 0, 255}, 90);
    if (sel) {
      gfx::fillCircle(r_, cx, cy, 40, kGold, 70);
      gfx::strokeCircle(r_, cx, cy, 38, 3, kGold);
    }
    drawChipSprite(cents, cx, cy, 64);
    drawChipLabel(ctx_.fonts.body, cents, cx, cy);
    drawText(ctx_.fonts.small, std::to_string(i + 1), sel ? kGold : kDim, cx, rc.bottom() + 18,
             Align::Center);
  }
  const bool betting2 = session_.phase() == Phase::Betting;
  const bool canDeal = betting2 && session_.canDeal() && !session_.isBroke();
  const bool hasBets = betting2 && session_.totalOnTable() > 0;
  drawButton(layout::clearButton(), "CLEAR", hasBets, SDL_Color{150, 70, 60, 255},
             hover(layout::clearButton()));
  drawButton(layout::rebetButton(), "REBET",
             betting2 && !session_.isBroke() && session_.totalOnTable() == 0 && hadPrevBets_,
             SDL_Color{50, 100, 160, 255}, hover(layout::rebetButton()));
  drawButton(layout::dealButton(), "DEAL", canDeal, SDL_Color{30, 140, 70, 255},
             hover(layout::dealButton()));
  drawText(ctx_.fonts.small,
           "Space deal   R rebet   Backspace clear   Shift+P/B/T remove   M mute   Esc quit",
           SDL_Color{150, 175, 160, 255}, 1180, 660, Align::Right);
}

void TableScene::drawHistory() {
  const auto& F = ctx_.fonts;
  drawText(F.small, "HISTORY", kDim, 60, 692);
  const auto& h = session_.history();
  const std::size_t n = std::min<std::size_t>(h.size(), 20);
  const std::size_t start = h.size() - n;
  for (std::size_t i = 0; i < n; ++i) {
    const Outcome o = h[start + i];
    const float x = 140 + float(i) * 33;
    const layout::Rect t{x, 676, 29, 29};
    gfx::fillRoundRect(r_, gfx::toF(t), 7, outcomeColor(o));
    const bool newest = i + 1 == n;
    if (newest) gfx::strokeRoundRect(r_, {t.x - 2, t.y - 2, t.w + 4, t.h + 4}, 9, 2, kGold);
    const char* l = o == Outcome::Player ? "P" : o == Outcome::Banker ? "B" : "T";
    drawText(F.small, l, kWhite, t.cx(), t.cy(), Align::Center);
  }
  // Session counts, right aligned.
  const std::string c[3] = {"P " + std::to_string(session_.playerWins()),
                            "B " + std::to_string(session_.bankerWins()),
                            "T " + std::to_string(session_.ties())};
  const SDL_Color cols[3] = {kPlayerCol, kBankerCol, kTieCol};
  float x = 1220;
  for (int i = 2; i >= 0; --i) {
    const float w = text_.width(F.bold, c[i]);
    drawText(F.bold, c[i], cols[i], x, 692, Align::Right);
    x -= w + 28;
  }
  drawText(F.small, "Session", kDim, x, 692, Align::Right);
}

void TableScene::drawToast() {
  if (toastT_ <= 0 || toastText_.empty()) return;
  const float a = float(std::min(1.0, toastT_ / 0.4));
  const auto t = layout::toastRect();
  gfx::fillRoundRect(r_, gfx::toF(t), 16, SDL_Color{0, 0, 0, 255}, Uint8(190 * a));
  gfx::strokeRoundRect(r_, gfx::toF(t), 16, 2, kGold, Uint8(255 * a));
  drawText(ctx_.fonts.body, toastText_, kWhite, t.cx(), t.cy(), Align::Center, Uint8(255 * a));
}

void TableScene::drawBrokeOverlay() {
  if (!session_.isBroke()) return;
  gfx::fillRoundRect(r_, {0, 0, layout::kWidth, layout::kHeight}, 0, SDL_Color{0, 0, 0, 255}, 175);
  const layout::Rect p{400, 232, 480, 240};
  gfx::fillRoundRect(r_, gfx::toF(p), 20, SDL_Color{14, 50, 34, 255});
  gfx::strokeRoundRect(r_, gfx::toF(p), 20, 3, kGold);
  drawShadowText(ctx_.fonts.title, "Out of chips", kWhite, p.cx(), p.y + 62);
  drawText(ctx_.fonts.body, "Your bankroll is empty.", kDim, p.cx(), p.y + 118, Align::Center);
  const auto b = layout::newSessionButton();
  drawButton(b, ("New session (" + formatMoneyShort(bac::rules::kStartingBankrollCents) + ")").c_str(),
             true, SDL_Color{30, 140, 70, 255}, hover(b));
}

void TableScene::render(SDL_Renderer* r) {
  drawFelt(r);
  drawHands();
  drawShoe();
  drawBanner();
  drawSpots();
  drawToast();
  drawTray();
  drawHistory();
  drawTopBar();
  drawBrokeOverlay();

  for (const std::string& path : shotPaths_) {
    SDL_Surface* s = SDL_RenderReadPixels(r, nullptr);
    if (s) {
      if (SDL_SaveBMP(s, path.c_str())) SDL_Log("script: saved screenshot %s", path.c_str());
      else SDL_Log("error: SDL_SaveBMP(%s) failed: %s", path.c_str(), SDL_GetError());
      SDL_DestroySurface(s);
    } else {
      SDL_Log("error: SDL_RenderReadPixels failed: %s", SDL_GetError());
    }
  }
  shotPaths_.clear();
}

}  // namespace bac::app
