#pragma once
// Pure deal timeline. Cards are dealt one after another: slide face down from the
// shoe, then flip. No SDL types. Card index = position in RoundResult::deals;
// index 4 and 5 are third cards (a short pause precedes the first one).

namespace bac::app {

struct DealTiming {
  double slide = 0.30;       // seconds, ease-out
  double flip = 0.20;        // seconds
  double gap = 0.05;         // pause between cards
  double thirdPause = 0.30;  // extra pause before the first third card
  int firstThirdIndex = 4;
};

struct CardAnim {
  enum class Stage { Waiting, Sliding, Flipping, Done };
  Stage stage = Stage::Waiting;
  bool visible = false;  // false while Waiting
  double slide = 0;      // eased 0..1 travel from the shoe to the slot
  double scaleX = 1;     // 1 -> 0 -> 1 across the flip
  bool faceUp = false;
};

class DealAnimator {
 public:
  explicit DealAnimator(int cardCount, DealTiming timing = {});
  int count() const { return count_; }
  double slideStart(int i) const;
  double flipStart(int i) const;
  double flipEnd(int i) const;
  double totalDuration() const;
  CardAnim at(int i, double t) const;
  bool finished(double t) const { return t >= totalDuration(); }

 private:
  int count_;
  DealTiming t_;
};

double easeOut(double p);  // cubic, clamps to [0,1]

}  // namespace bac::app
