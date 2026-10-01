#pragma once
// Pure text formatting for the UI (money, banner, net line). No SDL.

#include <cstdint>
#include <string>

#include "baccarat/rules/round.hpp"

namespace bac::app {

// "$1,000.00", "-$25.00", "$0.05"
std::string formatMoney(std::int64_t cents);
// Like formatMoney but drops ".00" for whole dollars: "$1,000", "$0.50".
std::string formatMoneyShort(std::int64_t cents);
// "+$95.00", "-$25.00", "$0.00"
std::string formatSignedMoney(std::int64_t cents);

// "PLAYER WINS 7 to 5", "BANKER WINS 6 to 4", "TIE" (+ " (Natural)" when natural).
std::string bannerText(bac::rules::Outcome outcome, int playerTotal, int bankerTotal,
                       bool natural);

// Net result line. totalStake 0 -> "No bets placed". net 0 -> "Even: $0.00".
// commissionCents > 0 adds " (after $5.00 commission)".
std::string netText(std::int64_t totalStake, std::int64_t net, std::int64_t commissionCents);

}  // namespace bac::app
