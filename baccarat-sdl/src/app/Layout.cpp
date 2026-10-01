#include "Layout.hpp"

namespace bac::app::layout {
namespace {
constexpr float kSpotW = 320, kSpotH = 170, kSpotY = 345;
constexpr float kSpotX[kSpotCount] = {100, 860, 480};  // Player, Banker, Tie
constexpr float kChipSize = 64, kChipY = 580, kChipX0 = 110, kChipStep = 92;
}  // namespace

Rect spotRect(int spot) { return {kSpotX[spot], kSpotY, kSpotW, kSpotH}; }

Rect spotRemoveRect(int spot) {
  Rect s = spotRect(spot);
  return {s.right() - 40, s.y + 10, 30, 30};
}

Rect chipRect(int index) { return {kChipX0 + kChipStep * index, kChipY, kChipSize, kChipSize}; }

Rect chipHitRect(int index) {
  Rect r = chipRect(index);
  return {r.x, r.y - kChipRaise, r.w, r.h + kChipRaise};
}

Rect dealButton() { return {1000, 576, 180, 60}; }
Rect clearButton() { return {660, 576, 150, 60}; }
Rect rebetButton() { return {830, 576, 150, 60}; }
Rect newSessionButton() { return {490, 388, 300, 56}; }
Rect muteRect() { return {1060, 6, 196, 36}; }
Rect shoeRect() { return {1150, 60, 100, 136}; }

float handCenterX(int side) { return side == 0 ? 380.0f : 860.0f; }

Rect cardRect(int side, int index) {
  const float cx = handCenterX(side);
  const float y = 104;
  const float step = kCardW + 12;
  // Two cards sit centred; the third is placed beside them.
  const float x0 = cx - kCardW - 6;
  return {x0 + step * index, y, kCardW, kCardH};
}

Rect bannerRect() { return {320, 264, 640, 72}; }
Rect toastRect() { return {390, 520, 500, 34}; }

Hit hitTest(float x, float y, bool broke) {
  if (broke) {
    if (newSessionButton().contains(x, y)) return {HitKind::NewSession, -1};
    return {};
  }
  if (dealButton().contains(x, y)) return {HitKind::Deal, -1};
  if (clearButton().contains(x, y)) return {HitKind::Clear, -1};
  if (rebetButton().contains(x, y)) return {HitKind::Rebet, -1};
  if (muteRect().contains(x, y)) return {HitKind::Mute, -1};
  for (int i = 0; i < kChipCount; ++i)
    if (chipHitRect(i).contains(x, y)) return {HitKind::Chip, i};
  for (int s = 0; s < kSpotCount; ++s)
    if (spotRemoveRect(s).contains(x, y)) return {HitKind::SpotRemove, s};
  for (int s = 0; s < kSpotCount; ++s)
    if (spotRect(s).contains(x, y)) return {HitKind::Spot, s};
  return {};
}

}  // namespace bac::app::layout
