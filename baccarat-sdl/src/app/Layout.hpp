#pragma once
// Pure layout and hit-testing for the table (1280x720 logical). No SDL.
// Spot indices follow bac::rules::kAllBetSpots (0 Player, 1 Banker, 2 Tie).
// Side indices follow bac::rules::Side (0 Player, 1 Banker).

namespace bac::app::layout {

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  float cx() const { return x + w / 2; }
  float cy() const { return y + h / 2; }
};

inline constexpr float kWidth = 1280.0f;
inline constexpr float kHeight = 720.0f;
inline constexpr float kBarHeight = 48.0f;
inline constexpr float kCardW = 112.0f;
inline constexpr float kCardH = 152.0f;
inline constexpr float kChipRaise = 14.0f;
inline constexpr int kChipCount = 4;
inline constexpr int kSpotCount = 3;

Rect spotRect(int spot);
Rect spotRemoveRect(int spot);   // small "-" control, top-right of the spot
Rect chipRect(int index);        // resting position (not raised)
Rect chipHitRect(int index);     // includes the raised area
Rect dealButton();
Rect clearButton();
Rect rebetButton();
Rect newSessionButton();         // in the "Out of chips" overlay
Rect muteRect();
Rect shoeRect();                 // where cards slide from
float handCenterX(int side);
Rect cardRect(int side, int index);  // final slot of card 0..2 of a hand
Rect bannerRect();
Rect toastRect();

enum class HitKind { None, Spot, SpotRemove, Chip, Deal, Clear, Rebet, NewSession, Mute };
struct Hit {
  HitKind kind = HitKind::None;
  int index = -1;  // spot or chip index
  friend bool operator==(const Hit&, const Hit&) = default;
};

// When `broke` is true the overlay is modal: only NewSession can be hit.
Hit hitTest(float x, float y, bool broke);

}  // namespace bac::app::layout
