#include "DealAnimator.hpp"

#include <algorithm>

namespace bac::app {

double easeOut(double p) {
  p = std::clamp(p, 0.0, 1.0);
  const double q = 1.0 - p;
  return 1.0 - q * q * q;
}

DealAnimator::DealAnimator(int cardCount, DealTiming timing)
    : count_(std::max(0, cardCount)), t_(timing) {}

double DealAnimator::slideStart(int i) const {
  double s = 0;
  for (int k = 1; k <= i; ++k) {
    s += t_.slide + t_.flip + t_.gap;
    if (k == t_.firstThirdIndex) s += t_.thirdPause;
  }
  return s;
}

double DealAnimator::flipStart(int i) const { return slideStart(i) + t_.slide; }
double DealAnimator::flipEnd(int i) const { return flipStart(i) + t_.flip; }

double DealAnimator::totalDuration() const {
  return count_ == 0 ? 0.0 : flipEnd(count_ - 1);
}

CardAnim DealAnimator::at(int i, double t) const {
  CardAnim a;
  if (i < 0 || i >= count_) return a;
  if (t < slideStart(i)) return a;  // Waiting
  a.visible = true;
  if (t < flipStart(i)) {
    a.stage = CardAnim::Stage::Sliding;
    a.slide = easeOut((t - slideStart(i)) / t_.slide);
    return a;
  }
  a.slide = 1.0;
  if (t < flipEnd(i)) {
    a.stage = CardAnim::Stage::Flipping;
    const double p = std::clamp((t - flipStart(i)) / t_.flip, 0.0, 1.0);
    a.faceUp = p > 0.5;
    a.scaleX = p < 0.5 ? 1.0 - 2.0 * p : 2.0 * p - 1.0;
    return a;
  }
  a.stage = CardAnim::Stage::Done;
  a.faceUp = true;
  a.scaleX = 1.0;
  return a;
}

}  // namespace bac::app
