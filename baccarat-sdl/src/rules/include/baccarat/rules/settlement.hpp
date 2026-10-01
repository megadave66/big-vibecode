#pragma once
// Bet settlement in integer cents.

#include <array>
#include <cstdint>
#include <string>

#include "baccarat/rules/round.hpp"

namespace bac::rules {

enum class BetSpot { Player, Banker, Tie };
inline constexpr std::array<BetSpot, 3> kAllBetSpots{BetSpot::Player, BetSpot::Banker,
                                                     BetSpot::Tie};

enum class BetResult { NoBet, Win, Lose, Push };

struct Bets {
    std::int64_t player = 0;
    std::int64_t banker = 0;
    std::int64_t tie = 0;

    std::int64_t total() const { return player + banker + tie; }
    std::int64_t at(BetSpot s) const;
    std::int64_t& at(BetSpot s);
};

struct SpotSettlement {
    BetSpot spot = BetSpot::Player;
    std::int64_t stake = 0;
    BetResult result = BetResult::NoBet;
    std::int64_t commissionCents = 0;  // only on a Banker win
    std::int64_t returnedCents = 0;    // back to bankroll: stake + net winnings, or 0
    std::int64_t netCents = 0;         // returned - stake
};

struct Settlement {
    std::int64_t returnedCents = 0;   // total paid back to the bankroll
    std::int64_t netCents = 0;        // returned - total stakes
    std::int64_t commissionCents = 0;
    std::array<SpotSettlement, 3> spots{};  // indexed in kAllBetSpots order

    const SpotSettlement& at(BetSpot s) const { return spots[static_cast<int>(s)]; }
};

std::int64_t commissionFor(std::int64_t bankerWinningsCents);  // floor(w * 5 / 100)

// Net winnings (after commission) for a winning stake on this spot.
// Player: stake; Banker: stake - commission; Tie: 8 * stake.
std::int64_t payoutFor(BetSpot spot, std::int64_t stakeCents);

// "1:1", "1:1 (5% comm.)", "8:1" — derived from the constants.
std::string payoutLabel(BetSpot spot);

// Negative stakes throw std::invalid_argument.
// Win: stake + payoutFor. Lose: 0. On Tie, Player/Banker push (stake back).
Settlement settle(const Bets& bets, Outcome outcome);

}  // namespace bac::rules
