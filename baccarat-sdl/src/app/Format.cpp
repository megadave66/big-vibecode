#include "Format.hpp"

namespace bac::app {
namespace {
std::string group(std::int64_t v) {
  std::string s = std::to_string(v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return s;
}
}  // namespace

std::string formatMoney(std::int64_t cents) {
  const bool neg = cents < 0;
  const std::int64_t a = neg ? -cents : cents;
  const std::int64_t c = a % 100;
  std::string out = neg ? "-$" : "$";
  out += group(a / 100);
  out += '.';
  if (c < 10) out += '0';
  out += std::to_string(c);
  return out;
}

std::string formatMoneyShort(std::int64_t cents) {
  if (cents % 100 == 0) {
    const bool neg = cents < 0;
    return std::string(neg ? "-$" : "$") + group((neg ? -cents : cents) / 100);
  }
  return formatMoney(cents);
}

std::string formatSignedMoney(std::int64_t cents) {
  if (cents > 0) return "+" + formatMoney(cents);
  return formatMoney(cents);
}

std::string bannerText(bac::rules::Outcome outcome, int playerTotal, int bankerTotal,
                       bool natural) {
  using bac::rules::Outcome;
  std::string s;
  switch (outcome) {
    case Outcome::Player:
      s = "PLAYER WINS " + std::to_string(playerTotal) + " to " + std::to_string(bankerTotal);
      break;
    case Outcome::Banker:
      s = "BANKER WINS " + std::to_string(bankerTotal) + " to " + std::to_string(playerTotal);
      break;
    case Outcome::Tie:
      s = "TIE";
      break;
  }
  if (natural) s += " (Natural)";
  return s;
}

std::string netText(std::int64_t totalStake, std::int64_t net, std::int64_t commissionCents) {
  if (totalStake <= 0) return "No bets placed";
  std::string s;
  if (net == 0) {
    s = "Even: $0.00";
  } else {
    s = formatSignedMoney(net);
  }
  if (commissionCents > 0) s += " (after " + formatMoney(commissionCents) + " commission)";
  return s;
}

}  // namespace bac::app
