#include <doctest/doctest.h>

#include <cmath>

#include "DealAnimator.hpp"
#include "Format.hpp"
#include "Layout.hpp"

using namespace bac::app;
using namespace bac::app::layout;
using bac::rules::Outcome;

TEST_SUITE("Format") {
  TEST_CASE("formatMoney") {
    CHECK(formatMoney(0) == "$0.00");
    CHECK(formatMoney(5) == "$0.05");
    CHECK(formatMoney(100) == "$1.00");
    CHECK(formatMoney(100000) == "$1,000.00");
    CHECK(formatMoney(123456789) == "$1,234,567.89");
    CHECK(formatMoney(-2500) == "-$25.00");
    CHECK(formatMoney(99900) == "$999.00");
    CHECK(formatMoney(-5) == "-$0.05");
  }
  TEST_CASE("formatMoneyShort") {
    CHECK(formatMoneyShort(100000) == "$1,000");
    CHECK(formatMoneyShort(50) == "$0.50");
    CHECK(formatMoneyShort(2500) == "$25");
  }
  TEST_CASE("formatSignedMoney") {
    CHECK(formatSignedMoney(9500) == "+$95.00");
    CHECK(formatSignedMoney(-2500) == "-$25.00");
    CHECK(formatSignedMoney(0) == "$0.00");
  }
  TEST_CASE("bannerText") {
    CHECK(bannerText(Outcome::Player, 7, 5, false) == "PLAYER WINS 7 to 5");
    CHECK(bannerText(Outcome::Banker, 4, 6, false) == "BANKER WINS 6 to 4");
    CHECK(bannerText(Outcome::Tie, 5, 5, false) == "TIE");
    CHECK(bannerText(Outcome::Player, 9, 3, true) == "PLAYER WINS 9 to 3 (Natural)");
    CHECK(bannerText(Outcome::Tie, 8, 8, true) == "TIE (Natural)");
  }
  TEST_CASE("netText") {
    CHECK(netText(0, 0, 0) == "No bets placed");
    CHECK(netText(2500, -2500, 0) == "-$25.00");
    CHECK(netText(10000, 9500, 500) == "+$95.00 (after $5.00 commission)");
    CHECK(netText(5000, 0, 0) == "Even: $0.00");
  }
}

TEST_SUITE("Layout") {
  TEST_CASE("Rect::contains is half-open") {
    Rect r{10, 20, 30, 40};
    CHECK(r.contains(10, 20));
    CHECK(r.contains(39.9f, 59.9f));
    CHECK_FALSE(r.contains(40, 30));
    CHECK_FALSE(r.contains(9.9f, 30));
  }
  TEST_CASE("hit-test spots, chips, buttons") {
    for (int s = 0; s < kSpotCount; ++s) {
      Rect r = spotRect(s);
      Hit h = hitTest(r.cx(), r.cy(), false);
      CHECK(h.kind == HitKind::Spot);
      CHECK(h.index == s);
      Rect rm = spotRemoveRect(s);
      h = hitTest(rm.cx(), rm.cy(), false);
      CHECK(h.kind == HitKind::SpotRemove);
      CHECK(h.index == s);
    }
    for (int i = 0; i < kChipCount; ++i) {
      Rect r = chipRect(i);
      Hit h = hitTest(r.cx(), r.cy(), false);
      CHECK(h.kind == HitKind::Chip);
      CHECK(h.index == i);
      // raised chip area still hits
      CHECK(hitTest(r.cx(), r.y - 5, false).kind == HitKind::Chip);
    }
    CHECK(hitTest(dealButton().cx(), dealButton().cy(), false).kind == HitKind::Deal);
    CHECK(hitTest(clearButton().cx(), clearButton().cy(), false).kind == HitKind::Clear);
    CHECK(hitTest(rebetButton().cx(), rebetButton().cy(), false).kind == HitKind::Rebet);
    CHECK(hitTest(muteRect().cx(), muteRect().cy(), false).kind == HitKind::Mute);
    CHECK(hitTest(5, 300, false).kind == HitKind::None);
  }
  TEST_CASE("broke overlay is modal") {
    Rect r = spotRect(0);
    CHECK(hitTest(r.cx(), r.cy(), true).kind == HitKind::None);
    CHECK(hitTest(dealButton().cx(), dealButton().cy(), true).kind == HitKind::None);
    CHECK(hitTest(newSessionButton().cx(), newSessionButton().cy(), true).kind ==
          HitKind::NewSession);
    // Without the overlay the same point is just the Tie spot underneath.
    CHECK(hitTest(newSessionButton().cx(), newSessionButton().cy(), false).kind ==
          HitKind::Spot);
  }
  TEST_CASE("no overlaps between interactive areas") {
    auto overlap = [](const Rect& a, const Rect& b) {
      return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
    };
    for (int a = 0; a < kSpotCount; ++a)
      for (int b = a + 1; b < kSpotCount; ++b) CHECK_FALSE(overlap(spotRect(a), spotRect(b)));
    for (int s = 0; s < kSpotCount; ++s) {
      CHECK_FALSE(overlap(spotRect(s), dealButton()));
      CHECK_FALSE(overlap(spotRect(s), toastRect()));
      CHECK_FALSE(overlap(spotRect(s), bannerRect()));
    }
    CHECK_FALSE(overlap(dealButton(), rebetButton()));
    CHECK_FALSE(overlap(clearButton(), rebetButton()));
    CHECK_FALSE(overlap(chipHitRect(3), clearButton()));
    CHECK_FALSE(overlap(bannerRect(), cardRect(0, 2)));
    CHECK_FALSE(overlap(cardRect(0, 2), cardRect(1, 0)));
    CHECK_FALSE(overlap(cardRect(1, 2), shoeRect()));
  }
  TEST_CASE("everything fits the logical screen") {
    auto inside = [](const Rect& r) {
      return r.x >= 0 && r.y >= 0 && r.right() <= kWidth && r.bottom() <= kHeight;
    };
    for (int s = 0; s < kSpotCount; ++s) CHECK(inside(spotRect(s)));
    for (int i = 0; i < kChipCount; ++i) CHECK(inside(chipHitRect(i)));
    for (int side = 0; side < 2; ++side)
      for (int i = 0; i < 3; ++i) CHECK(inside(cardRect(side, i)));
    CHECK(inside(dealButton()));
    CHECK(inside(muteRect()));
    CHECK(inside(shoeRect()));
  }
}

TEST_SUITE("DealAnimator") {
  TEST_CASE("start times are strictly increasing and sequential") {
    DealAnimator a(6);
    for (int i = 1; i < 6; ++i) CHECK(a.slideStart(i) >= a.flipEnd(i - 1));
    CHECK(a.slideStart(0) == doctest::Approx(0.0));
    CHECK(a.flipStart(0) == doctest::Approx(0.30));
    CHECK(a.flipEnd(0) == doctest::Approx(0.50));
  }
  TEST_CASE("third card has an extra pause") {
    DealAnimator a(5);
    const double normalGap = a.slideStart(3) - a.flipEnd(2);
    const double thirdGap = a.slideStart(4) - a.flipEnd(3);
    CHECK(thirdGap == doctest::Approx(normalGap + 0.30));
  }
  TEST_CASE("stages over time") {
    DealAnimator a(4);
    CHECK(a.at(0, -0.1).stage == CardAnim::Stage::Waiting);
    CHECK_FALSE(a.at(1, 0.1).visible);
    CardAnim s = a.at(0, 0.15);
    CHECK(s.stage == CardAnim::Stage::Sliding);
    CHECK(s.visible);
    CHECK_FALSE(s.faceUp);
    CHECK(s.slide > 0.5);  // ease-out is ahead of linear at the half-way point
    CHECK(s.slide < 1.0);
    CHECK(a.at(0, 0.30).stage == CardAnim::Stage::Flipping);
    CardAnim d = a.at(0, 0.60);
    CHECK(d.stage == CardAnim::Stage::Done);
    CHECK(d.faceUp);
    CHECK(d.scaleX == doctest::Approx(1.0));
  }
  TEST_CASE("flip scale goes 1 -> 0 -> 1 and swaps face at the middle") {
    DealAnimator a(1);
    CardAnim s = a.at(0, a.flipStart(0));
    CHECK(s.scaleX == doctest::Approx(1.0));
    CHECK_FALSE(s.faceUp);
    CardAnim m = a.at(0, a.flipStart(0) + 0.10 - 1e-9);
    CHECK(m.scaleX < 0.01);
    CHECK_FALSE(m.faceUp);
    CardAnim after = a.at(0, a.flipStart(0) + 0.15);
    CHECK(after.faceUp);
    CHECK(after.scaleX == doctest::Approx(0.5).epsilon(0.01));
    CHECK(a.at(0, a.flipEnd(0)).scaleX == doctest::Approx(1.0));
  }
  TEST_CASE("finished and duration") {
    DealAnimator a(4);
    CHECK(a.totalDuration() == doctest::Approx(a.flipEnd(3)));
    CHECK_FALSE(a.finished(a.totalDuration() - 0.01));
    CHECK(a.finished(a.totalDuration()));
    CHECK(DealAnimator(0).finished(0));
  }
  TEST_CASE("a six card round stays short") {
    DealAnimator a(6);
    CHECK(a.totalDuration() < 4.0);
  }
  TEST_CASE("out-of-range index is invisible") {
    DealAnimator a(2);
    CHECK_FALSE(a.at(5, 10).visible);
    CHECK_FALSE(a.at(-1, 10).visible);
  }
  TEST_CASE("easeOut is monotonic and clamped") {
    CHECK(easeOut(-1) == doctest::Approx(0));
    CHECK(easeOut(2) == doctest::Approx(1));
    double prev = 0;
    for (int i = 1; i <= 20; ++i) {
      double v = easeOut(i / 20.0);
      CHECK(v >= prev);
      prev = v;
    }
  }
}
