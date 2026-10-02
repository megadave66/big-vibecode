#include <doctest/doctest.h>

#include <vector>

#include "baccarat/game/session.hpp"

using namespace bac::game;
using namespace bac::rules;
using Status = PlaceResult::Status;

namespace {

Card C(Rank r) { return Card{r, Suit::Spades}; }

// Deal order: P1, B1, P2, B2.
std::vector<Card> playerWinRound() {  // P 9 (5+4) natural, B 3
    return {C(Rank::Five), C(Rank::Ace), C(Rank::Four), C(Rank::Two)};
}
std::vector<Card> bankerWinRound() {  // P 2, B 9 natural
    return {C(Rank::Ten), C(Rank::Four), C(Rank::Two), C(Rank::Five)};
}
std::vector<Card> tieRound() {  // P 8, B 8
    return {C(Rank::Four), C(Rank::Three), C(Rank::Four), C(Rank::Five)};
}
std::vector<Card> repeat(const std::vector<Card>& r, int n) {
    std::vector<Card> out;
    for (int i = 0; i < n; ++i) out.insert(out.end(), r.begin(), r.end());
    return out;
}
Session make(const std::vector<Card>& cards, std::size_t cut = 0) {
    return Session(Shoe::stacked(cards, cut));
}
void playThrough(Session& s) {
    REQUIRE(s.deal());
    REQUIRE(s.finishDealing());
    REQUIRE(s.collect());
    REQUIRE(s.nextRound());
}

}  // namespace

TEST_CASE("session: initial state") {
    Session s(42);
    CHECK(s.phase() == Phase::Betting);
    CHECK(s.bankroll() == kStartingBankrollCents);
    CHECK(s.selectedChip() == 100);
    CHECK(s.totalOnTable() == 0);
    CHECK_FALSE(s.canDeal());
    CHECK(s.history().empty());
    CHECK(s.playerWins() == 0);
    CHECK(s.bankerWins() == 0);
    CHECK(s.ties() == 0);
    CHECK_FALSE(s.isBroke());
    CHECK_FALSE(s.reshuffledThisRound());
}

TEST_CASE("session: chip selection validation") {
    Session s(1);
    for (auto c : kChipValuesCents) {
        CHECK(s.selectChip(c));
        CHECK(s.selectedChip() == c);
    }
    CHECK(s.selectedChip() == 10000);
    CHECK_FALSE(s.selectChip(0));
    CHECK_FALSE(s.selectChip(200));
    CHECK_FALSE(s.selectChip(-100));
    CHECK_FALSE(s.selectChip(1));
    CHECK(s.selectedChip() == 10000);
}

TEST_CASE("session: place, remove, clear and conservation") {
    Session s(1);
    const auto total = [&] { return s.bankroll() + s.totalOnTable(); };
    s.selectChip(2500);
    auto r = s.placeBet(BetSpot::Player);
    CHECK(r.status == Status::Placed);
    CHECK(r.placedCents == 2500);
    CHECK(s.bankroll() == kStartingBankrollCents - 2500);
    CHECK(total() == kStartingBankrollCents);
    s.selectChip(500);
    s.placeBet(BetSpot::Player);
    s.placeBet(BetSpot::Banker);
    s.selectChip(100);
    s.placeBet(BetSpot::Tie);
    CHECK(s.bets().player == 3000);
    CHECK(s.bets().banker == 500);
    CHECK(s.bets().tie == 100);
    CHECK(s.stack(BetSpot::Player) == std::vector<std::int64_t>{2500, 500});
    CHECK(total() == kStartingBankrollCents);
    CHECK(s.canDeal());

    // remove takes the LAST chip
    CHECK(s.removeBet(BetSpot::Player));
    CHECK(s.bets().player == 2500);
    CHECK(total() == kStartingBankrollCents);
    CHECK(s.removeBet(BetSpot::Tie));
    CHECK_FALSE(s.removeBet(BetSpot::Tie));  // now empty
    CHECK(s.placeBet(BetSpot::Player).status == Status::Placed);
    CHECK(s.removeAll(BetSpot::Player));
    CHECK(s.bets().player == 0);
    CHECK_FALSE(s.removeAll(BetSpot::Player));
    CHECK(total() == kStartingBankrollCents);
    CHECK(s.clearBets());
    CHECK(s.totalOnTable() == 0);
    CHECK(s.bankroll() == kStartingBankrollCents);
    CHECK_FALSE(s.clearBets());
    CHECK_FALSE(s.canDeal());
}

TEST_CASE("session: all-in and insufficient funds") {
    Session s(1);
    s.selectChip(10000);
    for (int i = 0; i < 10; ++i) CHECK(s.placeBet(BetSpot::Banker).status == Status::Placed);
    CHECK(s.bankroll() == 0);
    auto r = s.placeBet(BetSpot::Banker);
    CHECK(r.status == Status::InsufficientFunds);
    CHECK(r.placedCents == 0);
    CHECK_FALSE(s.isBroke());  // chips still on the table
    CHECK(s.clearBets());
    CHECK(s.bankroll() == kStartingBankrollCents);

    // Leave $25 in the bankroll.
    s.selectChip(10000);
    for (int i = 0; i < 9; ++i) s.placeBet(BetSpot::Player);
    s.selectChip(2500);
    for (int i = 0; i < 3; ++i) s.placeBet(BetSpot::Banker);
    CHECK(s.bankroll() == 2500);
    s.selectChip(10000);
    auto all = s.placeBet(BetSpot::Tie);
    CHECK(all.status == Status::AllIn);
    CHECK(all.placedCents == 2500);
    CHECK(s.bankroll() == 0);
    CHECK(s.bets().tie == 2500);
    CHECK(s.bankroll() + s.totalOnTable() == kStartingBankrollCents);
    // The all-in chip can be taken back whole.
    CHECK(s.removeBet(BetSpot::Tie));
    CHECK(s.bankroll() == 2500);
}

TEST_CASE("session: wrong-phase calls are rejected") {
    Session s = make(repeat(playerWinRound(), 3));
    CHECK_FALSE(s.deal());  // nothing on table
    CHECK_FALSE(s.finishDealing());
    CHECK_FALSE(s.collect());
    CHECK_FALSE(s.nextRound());
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    CHECK(s.phase() == Phase::Dealing);
    const auto bank = s.bankroll();
    CHECK(s.placeBet(BetSpot::Banker).status == Status::WrongPhase);
    CHECK_FALSE(s.removeBet(BetSpot::Player));
    CHECK_FALSE(s.removeAll(BetSpot::Player));
    CHECK_FALSE(s.clearBets());
    CHECK(s.rebet() == RebetStatus::WrongPhase);
    CHECK_FALSE(s.canDeal());
    CHECK_FALSE(s.deal());
    CHECK_FALSE(s.collect());
    CHECK_FALSE(s.nextRound());
    CHECK(s.bankroll() == bank);
    REQUIRE(s.finishDealing());
    CHECK(s.phase() == Phase::Resolution);
    CHECK_FALSE(s.finishDealing());
    CHECK_FALSE(s.deal());
    CHECK_FALSE(s.nextRound());
    CHECK(s.placeBet(BetSpot::Banker).status == Status::WrongPhase);
    CHECK_FALSE(s.removeBet(BetSpot::Player));
    REQUIRE(s.collect());
    CHECK(s.phase() == Phase::Payout);
    CHECK_FALSE(s.collect());
    CHECK_FALSE(s.finishDealing());
    CHECK_FALSE(s.deal());
    CHECK(s.placeBet(BetSpot::Banker).status == Status::WrongPhase);
    CHECK_FALSE(s.clearBets());
    REQUIRE(s.nextRound());
    CHECK(s.phase() == Phase::Betting);
    CHECK_FALSE(s.nextRound());
    CHECK(s.totalOnTable() == 0);
}

TEST_CASE("session: player win pays 1:1") {
    Session s = make(repeat(playerWinRound(), 3));
    s.selectChip(10000);
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    CHECK(s.lastRound().outcome == Outcome::Player);
    CHECK(s.lastRound().playerTotal == 9);
    CHECK(s.bankroll() == kStartingBankrollCents - 10000);
    REQUIRE(s.finishDealing());
    CHECK(s.lastSettlement().returnedCents == 20000);
    CHECK(s.lastNetCents() == 10000);
    CHECK(s.bankroll() == kStartingBankrollCents - 10000);  // not credited yet
    REQUIRE(s.collect());
    CHECK(s.bankroll() == kStartingBankrollCents + 10000);
    REQUIRE(s.nextRound());
    CHECK(s.totalOnTable() == 0);
    CHECK(s.playerWins() == 1);
    CHECK(s.bankerWins() == 0);
    CHECK(s.ties() == 0);
}

TEST_CASE("session: banker win pays 1:1 less 5% commission") {
    Session s = make(repeat(bankerWinRound(), 3));
    s.selectChip(10000);
    s.placeBet(BetSpot::Banker);
    REQUIRE(s.deal());
    CHECK(s.lastRound().outcome == Outcome::Banker);
    REQUIRE(s.finishDealing());
    CHECK(s.lastNetCents() == 9500);
    REQUIRE(s.collect());
    CHECK(s.bankroll() == kStartingBankrollCents + 9500);
    CHECK(s.bankerWins() == 1);
    // Player bet on a banker round loses.
    REQUIRE(s.nextRound());
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    REQUIRE(s.finishDealing());
    CHECK(s.lastNetCents() == -10000);
    REQUIRE(s.collect());
    CHECK(s.bankroll() == kStartingBankrollCents + 9500 - 10000);
}

TEST_CASE("session: tie pushes player/banker and pays tie 8:1") {
    Session s = make(repeat(tieRound(), 4));
    s.selectChip(10000);
    s.placeBet(BetSpot::Player);
    s.placeBet(BetSpot::Banker);
    s.selectChip(500);
    s.placeBet(BetSpot::Tie);
    REQUIRE(s.deal());
    CHECK(s.lastRound().outcome == Outcome::Tie);
    REQUIRE(s.finishDealing());
    CHECK(s.lastNetCents() == 4000);
    REQUIRE(s.collect());
    CHECK(s.bankroll() == kStartingBankrollCents + 4000);
    CHECK(s.ties() == 1);
    // Only P/B on a tie: pure push.
    REQUIRE(s.nextRound());
    s.selectChip(10000);
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    REQUIRE(s.finishDealing());
    CHECK(s.lastNetCents() == 0);
    REQUIRE(s.collect());
    CHECK(s.bankroll() == kStartingBankrollCents + 4000);
}

TEST_CASE("session: rebet") {
    Session s = make(repeat(playerWinRound(), 6));
    CHECK(s.rebet() == RebetStatus::NothingToRebet);
    s.selectChip(2500);
    s.placeBet(BetSpot::Player);
    s.placeBet(BetSpot::Player);
    s.selectChip(500);
    s.placeBet(BetSpot::Tie);
    // table not empty
    CHECK(s.rebet() == RebetStatus::TableNotEmpty);
    playThrough(s);
    const auto bank = s.bankroll();
    CHECK(s.rebet() == RebetStatus::Placed);
    CHECK(s.bets().player == 5000);
    CHECK(s.bets().tie == 500);
    CHECK(s.bankroll() == bank - 5500);
    CHECK(s.stack(BetSpot::Player) == std::vector<std::int64_t>{2500, 2500});
    CHECK(s.rebet() == RebetStatus::TableNotEmpty);
    s.clearBets();
    CHECK(s.bankroll() == bank);
    CHECK(s.rebet() == RebetStatus::Placed);  // previous round's bets remembered
}

TEST_CASE("session: rebet partial when bankroll is short") {
    Session s = make(repeat(bankerWinRound(), 6));
    s.selectChip(10000);
    for (int i = 0; i < 9; ++i) s.placeBet(BetSpot::Player);  // 90000 on player
    s.placeBet(BetSpot::Banker);                               // 10000, bankroll 0
    CHECK(s.bankroll() == 0);
    REQUIRE(s.deal());
    REQUIRE(s.finishDealing());
    // Banker won: banker bet pays 19500 back, player lost.
    REQUIRE(s.collect());
    REQUIRE(s.nextRound());
    CHECK(s.bankroll() == 19500);
    // Previous: 9 x $100 player, 1 x $100 banker. Fit: 1 chip on player... 19500 / 10000 = 1
    CHECK(s.rebet() == RebetStatus::Partial);
    CHECK(s.bets().player == 10000);
    CHECK(s.bets().banker == 0);
    CHECK(s.bankroll() == 9500);
}

TEST_CASE("session: rebet with empty bankroll places nothing") {
    Session s = make(repeat(bankerWinRound(), 6));
    s.selectChip(10000);
    for (int i = 0; i < 10; ++i) s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    REQUIRE(s.finishDealing());
    REQUIRE(s.collect());
    REQUIRE(s.nextRound());
    CHECK(s.bankroll() == 0);
    CHECK(s.isBroke());
    CHECK(s.rebet() == RebetStatus::NothingToRebet);
    CHECK(s.totalOnTable() == 0);
}

TEST_CASE("session: history capped at 30, counts accumulate") {
    Session s = make(repeat(playerWinRound(), 40));
    for (int i = 0; i < 35; ++i) {
        s.placeBet(BetSpot::Player);
        playThrough(s);
    }
    CHECK(s.history().size() == kHistoryMax);
    CHECK(s.playerWins() == 35);
    for (auto o : s.history()) CHECK(o == Outcome::Player);
    CHECK(s.bankroll() == kStartingBankrollCents + 35 * 100);
}

TEST_CASE("session: history order and counts for mixed outcomes") {
    std::vector<Card> cards = playerWinRound();
    for (auto* r : {&bankerWinRound, &tieRound}) {
        auto v = (*r)();
        cards.insert(cards.end(), v.begin(), v.end());
    }
    Session s = make(cards);
    for (int i = 0; i < 3; ++i) {
        s.placeBet(BetSpot::Player);
        playThrough(s);
    }
    REQUIRE(s.history().size() == 3);
    CHECK(s.history()[0] == Outcome::Player);
    CHECK(s.history()[1] == Outcome::Banker);
    CHECK(s.history()[2] == Outcome::Tie);
    CHECK(s.playerWins() == 1);
    CHECK(s.bankerWins() == 1);
    CHECK(s.ties() == 1);
}

TEST_CASE("session: isBroke and resetSession") {
    Session s = make(repeat(bankerWinRound(), 6));
    s.selectChip(10000);
    for (int i = 0; i < 10; ++i) s.placeBet(BetSpot::Player);
    CHECK_FALSE(s.isBroke());
    REQUIRE(s.deal());
    CHECK_FALSE(s.isBroke());  // not Betting
    REQUIRE(s.finishDealing());
    REQUIRE(s.collect());
    CHECK_FALSE(s.isBroke());
    REQUIRE(s.nextRound());
    CHECK(s.isBroke());
    CHECK(s.history().size() == 1);
    CHECK(s.placeBet(BetSpot::Player).status == Status::InsufficientFunds);

    s.resetSession();
    CHECK(s.phase() == Phase::Betting);
    CHECK(s.bankroll() == kStartingBankrollCents);
    CHECK(s.history().empty());
    CHECK(s.bankerWins() == 0);
    CHECK(s.totalOnTable() == 0);
    CHECK_FALSE(s.isBroke());
    CHECK(s.rebet() == RebetStatus::NothingToRebet);
    // Same shoe again: same first round.
    s.placeBet(BetSpot::Banker);
    REQUIRE(s.deal());
    CHECK(s.lastRound().outcome == Outcome::Banker);
}

TEST_CASE("session: reshuffle flag when the shoe passes the cut card") {
    // 12 cards, cut at 10 remaining; each round uses 4.
    Session s = make(repeat(playerWinRound(), 3), 10);
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    CHECK_FALSE(s.reshuffledThisRound());  // 12 remaining
    REQUIRE(s.finishDealing());
    REQUIRE(s.collect());
    REQUIRE(s.nextRound());
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    CHECK(s.reshuffledThisRound());  // 8 remaining < 10
    CHECK(s.shoe().remaining() == 8);  // restored to 12, minus 4
    REQUIRE(s.finishDealing());
    REQUIRE(s.collect());
    REQUIRE(s.nextRound());
    CHECK(s.reshuffledThisRound());  // sticky until the next deal
    s.placeBet(BetSpot::Player);
    REQUIRE(s.deal());
    CHECK(s.reshuffledThisRound());  // 8 again
}

TEST_CASE("session: real seeded shoe plays rounds and is deterministic") {
    Session a(7), b(7);
    for (int i = 0; i < 20; ++i) {
        a.placeBet(BetSpot::Banker);
        b.placeBet(BetSpot::Banker);
        playThrough(a);
        playThrough(b);
        CHECK(a.lastRound().outcome == b.lastRound().outcome);
        CHECK(a.bankroll() == b.bankroll());
    }
    CHECK(a.playerWins() + a.bankerWins() + a.ties() == 20);
}
