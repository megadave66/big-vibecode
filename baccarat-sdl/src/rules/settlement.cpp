#include "baccarat/rules/settlement.hpp"

#include <stdexcept>

#include "baccarat/rules/constants.hpp"

namespace bac::rules {
namespace {

Payout payoutRatio(BetSpot s) {
    switch (s) {
        case BetSpot::Player: return kPlayerPayout;
        case BetSpot::Banker: return kBankerPayout;
        case BetSpot::Tie: return kTiePayout;
    }
    return kPlayerPayout;
}

bool spotWins(BetSpot s, Outcome o) {
    switch (s) {
        case BetSpot::Player: return o == Outcome::Player;
        case BetSpot::Banker: return o == Outcome::Banker;
        case BetSpot::Tie: return o == Outcome::Tie;
    }
    return false;
}

}  // namespace

std::int64_t Bets::at(BetSpot s) const {
    switch (s) {
        case BetSpot::Player: return player;
        case BetSpot::Banker: return banker;
        case BetSpot::Tie: return tie;
    }
    return 0;
}

std::int64_t& Bets::at(BetSpot s) {
    switch (s) {
        case BetSpot::Player: return player;
        case BetSpot::Banker: return banker;
        case BetSpot::Tie: break;
    }
    return tie;
}

std::int64_t commissionFor(std::int64_t bankerWinningsCents) {
    if (bankerWinningsCents < 0) throw std::invalid_argument("negative winnings");
    // Integer division of non-negatives is floor.
    return bankerWinningsCents * kBankerCommissionPercent / 100;
}

std::int64_t payoutFor(BetSpot spot, std::int64_t stakeCents) {
    if (stakeCents < 0) throw std::invalid_argument("negative stake");
    const Payout p = payoutRatio(spot);
    const std::int64_t gross = stakeCents * p.num / p.den;
    return spot == BetSpot::Banker ? gross - commissionFor(gross) : gross;
}

std::string payoutLabel(BetSpot spot) {
    const Payout p = payoutRatio(spot);
    std::string label = std::to_string(p.num) + ":" + std::to_string(p.den);
    if (spot == BetSpot::Banker)
        label += " (" + std::to_string(kBankerCommissionPercent) + "% comm.)";
    return label;
}

Settlement settle(const Bets& bets, Outcome outcome) {
    Settlement s;
    for (BetSpot spot : kAllBetSpots) {
        SpotSettlement& ss = s.spots[static_cast<int>(spot)];
        ss.spot = spot;
        ss.stake = bets.at(spot);
        if (ss.stake < 0) throw std::invalid_argument("negative stake");
        if (ss.stake == 0) {
            ss.result = BetResult::NoBet;
        } else if (spotWins(spot, outcome)) {
            ss.result = BetResult::Win;
            const Payout p = payoutRatio(spot);
            const std::int64_t gross = ss.stake * p.num / p.den;
            ss.commissionCents = spot == BetSpot::Banker ? commissionFor(gross) : 0;
            ss.returnedCents = ss.stake + gross - ss.commissionCents;
        } else if (outcome == Outcome::Tie && spot != BetSpot::Tie) {
            ss.result = BetResult::Push;
            ss.returnedCents = ss.stake;
        } else {
            ss.result = BetResult::Lose;
            ss.returnedCents = 0;
        }
        ss.netCents = ss.returnedCents - ss.stake;
        s.returnedCents += ss.returnedCents;
        s.netCents += ss.netCents;
        s.commissionCents += ss.commissionCents;
    }
    return s;
}

}  // namespace bac::rules
