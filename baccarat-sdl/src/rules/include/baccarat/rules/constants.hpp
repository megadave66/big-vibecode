#pragma once
// bac::rules constants — the single source of truth for every rule number.
// UI and game code must read these; never hardcode them.

#include <array>
#include <cstddef>
#include <cstdint>

namespace bac::rules {

// A payout ratio "num:den" applied to the stake: winnings = stake * num / den.
struct Payout {
    int num;
    int den;
};

inline constexpr int kNumDecks = 8;
inline constexpr int kCardsPerDeck = 52;

inline constexpr Payout kPlayerPayout{1, 1};
inline constexpr Payout kBankerPayout{1, 1};
inline constexpr Payout kTiePayout{8, 1};

// Commission charged on Banker winnings. Rounding: floor(winnings * 5 / 100),
// computed in integer cents. Floor favours the player; whole-dollar stakes
// are exact (100 cents * 5% = 5 cents).
inline constexpr int kBankerCommissionPercent = 5;

inline constexpr std::int64_t kStartingBankrollCents = 100000;  // $1,000

inline constexpr std::array<std::int64_t, 4> kChipValuesCents{100, 500, 2500, 10000};

// Cut card: casino convention places the cut card about one deck from the
// back of the shoe. A new round must not start when fewer than this many
// cards remain; the shoe is reshuffled first. One round uses at most 6 cards,
// so a round can never run the shoe dry.
inline constexpr std::size_t kCutCardRemaining = 52;

// Burn on a new shoe: the first card is turned face up and that many further
// cards are burned. Ace = 1, 2-9 face value, 10/J/Q/K = kBurnFaceValue.
inline constexpr int kBurnFaceValue = 10;

inline constexpr int kMaxCardsPerRound = 6;

}  // namespace bac::rules
