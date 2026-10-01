#pragma once
// bac::game::Session — the round state machine. Pure C++20, no SDL.
// Betting -> deal() -> Dealing -> finishDealing() -> Resolution
//         -> collect() -> Payout -> nextRound() -> Betting.
// All rules (totals, draws, outcome, payouts) come from bac::rules.

#include <cstdint>
#include <deque>
#include <vector>

#include "baccarat/rules/rules.hpp"

namespace bac::game {

enum class Phase { Betting, Dealing, Resolution, Payout };

struct PlaceResult {
    enum class Status { Placed, AllIn, InsufficientFunds, WrongPhase };
    Status status = Status::WrongPhase;
    std::int64_t placedCents = 0;
};

enum class RebetStatus {
    Placed,        // everything from last round was placed
    Partial,       // some chips did not fit the bankroll
    NothingToRebet,// no previous bets (or none affordable: bankroll 0)
    TableNotEmpty,
    WrongPhase
};

inline constexpr std::size_t kHistoryMax = 30;

class Session {
public:
    explicit Session(std::uint64_t seed);
    explicit Session(bac::rules::Shoe shoe);  // deterministic tests

    // --- phase / flow ---
    Phase phase() const { return phase_; }
    bool canDeal() const;
    bool deal();           // Betting -> Dealing
    bool finishDealing();  // Dealing -> Resolution (settles, updates history)
    bool collect();        // Resolution -> Payout (credits returnedCents)
    bool nextRound();      // Payout -> Betting (clears table)

    // --- chips and bets (Betting only for changes) ---
    bool selectChip(std::int64_t cents);  // must be in kChipValuesCents
    std::int64_t selectedChip() const { return chip_; }
    PlaceResult placeBet(bac::rules::BetSpot spot);
    bool removeBet(bac::rules::BetSpot spot);     // last chip on spot
    bool removeAll(bac::rules::BetSpot spot);     // everything on spot
    bool clearBets();
    RebetStatus rebet();
    // Chip amounts on a spot, in placement order.
    const std::vector<std::int64_t>& stack(bac::rules::BetSpot spot) const;

    // --- results ---
    const bac::rules::RoundResult& lastRound() const { return round_; }
    const bac::rules::Settlement& lastSettlement() const { return settlement_; }
    bool reshuffledThisRound() const { return reshuffled_; }
    std::int64_t lastNetCents() const { return settlement_.netCents; }

    // --- stats ---
    std::int64_t bankroll() const { return bankroll_; }
    bac::rules::Bets bets() const;
    std::int64_t totalOnTable() const { return bets().total(); }
    const std::deque<bac::rules::Outcome>& history() const { return history_; }
    int playerWins() const { return counts_[0]; }
    int bankerWins() const { return counts_[1]; }
    int ties() const { return counts_[2]; }
    bool isBroke() const;
    void resetSession();
    const bac::rules::Shoe& shoe() const { return shoe_; }

private:
    using Stack = std::vector<std::int64_t>;
    Stack& stackRef(bac::rules::BetSpot s) { return stacks_[static_cast<int>(s)]; }
    const Stack& stackRef(bac::rules::BetSpot s) const { return stacks_[static_cast<int>(s)]; }

    bac::rules::Shoe initialShoe_;
    bac::rules::Shoe shoe_;
    Phase phase_ = Phase::Betting;
    std::int64_t bankroll_ = bac::rules::kStartingBankrollCents;
    std::int64_t chip_ = bac::rules::kChipValuesCents[0];
    Stack stacks_[3];
    Stack prevStacks_[3];
    bac::rules::RoundResult round_;
    bac::rules::Settlement settlement_;
    bool reshuffled_ = false;
    std::deque<bac::rules::Outcome> history_;
    int counts_[3] = {0, 0, 0};
};

}  // namespace bac::game
