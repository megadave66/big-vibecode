#include "baccarat/game/session.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace bac::game {

using bac::rules::BetSpot;
using bac::rules::kAllBetSpots;

Session::Session(std::uint64_t seed) : initialShoe_(seed), shoe_(initialShoe_) {}
Session::Session(bac::rules::Shoe shoe) : initialShoe_(shoe), shoe_(std::move(shoe)) {}

bac::rules::Bets Session::bets() const {
    bac::rules::Bets b;
    for (BetSpot s : kAllBetSpots) {
        const Stack& st = stackRef(s);
        b.at(s) = std::accumulate(st.begin(), st.end(), std::int64_t{0});
    }
    return b;
}

const std::vector<std::int64_t>& Session::stack(BetSpot spot) const { return stackRef(spot); }

bool Session::canDeal() const { return phase_ == Phase::Betting && totalOnTable() > 0; }

bool Session::deal() {
    if (!canDeal()) return false;
    reshuffled_ = shoe_.reshuffleIfNeeded();
    round_ = bac::rules::playRound(shoe_);
    settlement_ = {};
    for (int i = 0; i < 3; ++i) prevStacks_[i] = stacks_[i];
    phase_ = Phase::Dealing;
    return true;
}

bool Session::finishDealing() {
    if (phase_ != Phase::Dealing) return false;
    settlement_ = bac::rules::settle(bets(), round_.outcome);
    history_.push_back(round_.outcome);
    while (history_.size() > kHistoryMax) history_.pop_front();
    ++counts_[static_cast<int>(round_.outcome)];
    phase_ = Phase::Resolution;
    return true;
}

bool Session::collect() {
    if (phase_ != Phase::Resolution) return false;
    bankroll_ += settlement_.returnedCents;
    phase_ = Phase::Payout;
    return true;
}

bool Session::nextRound() {
    if (phase_ != Phase::Payout) return false;
    for (auto& st : stacks_) st.clear();
    phase_ = Phase::Betting;
    return true;
}

bool Session::selectChip(std::int64_t cents) {
    if (std::find(bac::rules::kChipValuesCents.begin(), bac::rules::kChipValuesCents.end(),
                  cents) == bac::rules::kChipValuesCents.end())
        return false;
    chip_ = cents;
    return true;
}

PlaceResult Session::placeBet(BetSpot spot) {
    using S = PlaceResult::Status;
    if (phase_ != Phase::Betting) return {S::WrongPhase, 0};
    if (bankroll_ <= 0) return {S::InsufficientFunds, 0};
    const bool allIn = bankroll_ < chip_;
    const std::int64_t amount = allIn ? bankroll_ : chip_;
    bankroll_ -= amount;
    stackRef(spot).push_back(amount);
    return {allIn ? S::AllIn : S::Placed, amount};
}

bool Session::removeBet(BetSpot spot) {
    if (phase_ != Phase::Betting) return false;
    Stack& st = stackRef(spot);
    if (st.empty()) return false;
    bankroll_ += st.back();
    st.pop_back();
    return true;
}

bool Session::removeAll(BetSpot spot) {
    if (phase_ != Phase::Betting) return false;
    Stack& st = stackRef(spot);
    if (st.empty()) return false;
    for (std::int64_t c : st) bankroll_ += c;
    st.clear();
    return true;
}

bool Session::clearBets() {
    if (phase_ != Phase::Betting) return false;
    bool any = false;
    for (BetSpot s : kAllBetSpots) any = removeAll(s) || any;
    return any;
}

RebetStatus Session::rebet() {
    if (phase_ != Phase::Betting) return RebetStatus::WrongPhase;
    if (totalOnTable() > 0) return RebetStatus::TableNotEmpty;
    bool anyPrev = false;
    bool allPlaced = true;
    bool anyPlaced = false;
    for (int i = 0; i < 3; ++i) {
        for (std::int64_t chip : prevStacks_[i]) {
            anyPrev = true;
            if (bankroll_ >= chip) {
                bankroll_ -= chip;
                stacks_[i].push_back(chip);
                anyPlaced = true;
            } else {
                allPlaced = false;  // does not fit; try smaller chips after it
            }
        }
    }
    if (!anyPrev || !anyPlaced) return RebetStatus::NothingToRebet;
    return allPlaced ? RebetStatus::Placed : RebetStatus::Partial;
}

bool Session::isBroke() const {
    return phase_ == Phase::Betting && bankroll_ == 0 && totalOnTable() == 0;
}

void Session::resetSession() {
    shoe_ = initialShoe_;
    phase_ = Phase::Betting;
    bankroll_ = bac::rules::kStartingBankrollCents;
    chip_ = bac::rules::kChipValuesCents[0];
    for (auto& st : stacks_) st.clear();
    for (auto& st : prevStacks_) st.clear();
    round_ = {};
    settlement_ = {};
    reshuffled_ = false;
    history_.clear();
    counts_[0] = counts_[1] = counts_[2] = 0;
}

}  // namespace bac::game
