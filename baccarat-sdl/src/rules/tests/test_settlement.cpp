#include <doctest/doctest.h>

#include <stdexcept>

#include "baccarat/rules/rules.hpp"

using namespace bac::rules;

TEST_CASE("constants are the spec values") {
    CHECK(kNumDecks == 8);
    CHECK(kPlayerPayout.num == 1);
    CHECK(kPlayerPayout.den == 1);
    CHECK(kBankerPayout.num == 1);
    CHECK(kBankerPayout.den == 1);
    CHECK(kTiePayout.num == 8);
    CHECK(kTiePayout.den == 1);
    CHECK(kBankerCommissionPercent == 5);
    CHECK(kStartingBankrollCents == 100000);
    CHECK(kChipValuesCents == std::array<std::int64_t, 4>{100, 500, 2500, 10000});
    CHECK(kCutCardRemaining == 52);
}

TEST_CASE("payout labels come from constants") {
    CHECK(payoutLabel(BetSpot::Player) == "1:1");
    CHECK(payoutLabel(BetSpot::Banker) == "1:1 (5% comm.)");
    CHECK(payoutLabel(BetSpot::Tie) == "8:1");
}

TEST_CASE("every single bet x outcome ($10 stake)") {
    const std::int64_t st = 1000;
    struct Row { BetSpot spot; Outcome out; BetResult res; std::int64_t returned; std::int64_t comm; };
    const Row rows[] = {
        {BetSpot::Player, Outcome::Player, BetResult::Win, 2000, 0},
        {BetSpot::Player, Outcome::Banker, BetResult::Lose, 0, 0},
        {BetSpot::Player, Outcome::Tie, BetResult::Push, 1000, 0},
        {BetSpot::Banker, Outcome::Player, BetResult::Lose, 0, 0},
        {BetSpot::Banker, Outcome::Banker, BetResult::Win, 1950, 50},
        {BetSpot::Banker, Outcome::Tie, BetResult::Push, 1000, 0},
        {BetSpot::Tie, Outcome::Player, BetResult::Lose, 0, 0},
        {BetSpot::Tie, Outcome::Banker, BetResult::Lose, 0, 0},
        {BetSpot::Tie, Outcome::Tie, BetResult::Win, 9000, 0},
    };
    for (const Row& r : rows) {
        CAPTURE(static_cast<int>(r.spot));
        CAPTURE(static_cast<int>(r.out));
        Bets b;
        b.at(r.spot) = st;
        const Settlement s = settle(b, r.out);
        CHECK(s.returnedCents == r.returned);
        CHECK(s.netCents == r.returned - st);
        CHECK(s.commissionCents == r.comm);
        const SpotSettlement& ss = s.at(r.spot);
        CHECK(ss.spot == r.spot);
        CHECK(ss.stake == st);
        CHECK(ss.result == r.res);
        CHECK(ss.returnedCents == r.returned);
        CHECK(ss.netCents == r.returned - st);
        CHECK(ss.commissionCents == r.comm);
        for (BetSpot other : kAllBetSpots)
            if (other != r.spot) {
                CHECK(s.at(other).result == BetResult::NoBet);
                CHECK(s.at(other).returnedCents == 0);
            }
    }
}

TEST_CASE("banker commission on whole-dollar wins") {
    struct Row { std::int64_t stake; std::int64_t returned; std::int64_t comm; };
    const Row rows[] = {
        {100, 195, 5},          // $1
        {500, 975, 25},         // $5: 500 stake + 475 winnings
        {2500, 4875, 125},      // $25
        {10000, 19500, 500},    // $100
        {100000, 195000, 5000}, // $1000
    };
    for (const Row& r : rows) {
        CAPTURE(r.stake);
        const Settlement s = settle(Bets{0, r.stake, 0}, Outcome::Banker);
        CHECK(s.returnedCents == r.returned);
        CHECK(s.commissionCents == r.comm);
        CHECK(s.netCents == r.returned - r.stake);
        CHECK(payoutFor(BetSpot::Banker, r.stake) == r.stake - r.comm);
        CHECK(commissionFor(r.stake) == r.comm);
    }
}

TEST_CASE("odd-cent banker stakes: commission floors (player-friendly)") {
    struct Row { std::int64_t stake; std::int64_t comm; };
    const Row rows[] = {{1, 0}, {19, 0}, {20, 1}, {21, 1}, {39, 1}, {40, 2}, {99, 4}, {101, 5},
                        {119, 5}, {120, 6}, {2599, 129}};
    for (const Row& r : rows) {
        CAPTURE(r.stake);
        CHECK(commissionFor(r.stake) == r.comm);
        const Settlement s = settle(Bets{0, r.stake, 0}, Outcome::Banker);
        CHECK(s.commissionCents == r.comm);
        CHECK(s.returnedCents == 2 * r.stake - r.comm);
    }
    // Floor rule holds for every stake 0..10000 cents.
    for (std::int64_t st = 0; st <= 10000; ++st) {
        const std::int64_t c = commissionFor(st);
        if (!(c * 100 <= st * 5 && st * 5 < (c + 1) * 100)) {
            CAPTURE(st);
            CHECK(false);
        }
    }
}

TEST_CASE("payoutFor helpers") {
    CHECK(payoutFor(BetSpot::Player, 0) == 0);
    CHECK(payoutFor(BetSpot::Player, 2500) == 2500);
    CHECK(payoutFor(BetSpot::Banker, 2500) == 2375);
    CHECK(payoutFor(BetSpot::Tie, 2500) == 20000);
    CHECK(payoutFor(BetSpot::Tie, 1) == 8);
    CHECK_THROWS_AS(payoutFor(BetSpot::Player, -1), std::invalid_argument);
    CHECK_THROWS_AS(commissionFor(-1), std::invalid_argument);
}

TEST_CASE("combined multi-spot bets") {
    const Bets b{1000, 2000, 500};  // $10 P, $20 B, $5 T; total 3500
    CHECK(b.total() == 3500);
    SUBCASE("player wins") {
        const Settlement s = settle(b, Outcome::Player);
        CHECK(s.at(BetSpot::Player).result == BetResult::Win);
        CHECK(s.at(BetSpot::Banker).result == BetResult::Lose);
        CHECK(s.at(BetSpot::Tie).result == BetResult::Lose);
        CHECK(s.returnedCents == 2000);
        CHECK(s.netCents == -1500);
        CHECK(s.commissionCents == 0);
    }
    SUBCASE("banker wins") {
        const Settlement s = settle(b, Outcome::Banker);
        CHECK(s.at(BetSpot::Banker).returnedCents == 3900);
        CHECK(s.returnedCents == 3900);
        CHECK(s.netCents == 400);
        CHECK(s.commissionCents == 100);
    }
    SUBCASE("tie: P and B push, tie pays 8:1") {
        const Settlement s = settle(b, Outcome::Tie);
        CHECK(s.at(BetSpot::Player).result == BetResult::Push);
        CHECK(s.at(BetSpot::Banker).result == BetResult::Push);
        CHECK(s.at(BetSpot::Tie).result == BetResult::Win);
        CHECK(s.at(BetSpot::Player).netCents == 0);
        CHECK(s.at(BetSpot::Banker).netCents == 0);
        CHECK(s.at(BetSpot::Tie).returnedCents == 4500);
        CHECK(s.returnedCents == 1000 + 2000 + 4500);
        CHECK(s.netCents == 4000);
        CHECK(s.commissionCents == 0);
    }
}

TEST_CASE("player and banker both bet, tie pushes both") {
    const Settlement s = settle(Bets{500, 500, 0}, Outcome::Tie);
    CHECK(s.returnedCents == 1000);
    CHECK(s.netCents == 0);
    CHECK(s.at(BetSpot::Tie).result == BetResult::NoBet);
}

TEST_CASE("zero bets settle to zero") {
    for (Outcome o : {Outcome::Player, Outcome::Banker, Outcome::Tie}) {
        const Settlement s = settle(Bets{}, o);
        CHECK(s.returnedCents == 0);
        CHECK(s.netCents == 0);
        CHECK(s.commissionCents == 0);
        for (BetSpot sp : kAllBetSpots) CHECK(s.at(sp).result == BetResult::NoBet);
    }
}

TEST_CASE("odd-cent stakes on player and tie") {
    CHECK(settle(Bets{1, 0, 0}, Outcome::Player).returnedCents == 2);
    CHECK(settle(Bets{19, 0, 0}, Outcome::Player).returnedCents == 38);
    CHECK(settle(Bets{0, 0, 21}, Outcome::Tie).returnedCents == 21 * 9);
    CHECK(settle(Bets{21, 19, 1}, Outcome::Tie).returnedCents == 21 + 19 + 9);
}

TEST_CASE("negative stake throws") {
    CHECK_THROWS_AS(settle(Bets{-1, 0, 0}, Outcome::Player), std::invalid_argument);
    CHECK_THROWS_AS(settle(Bets{0, 0, -5}, Outcome::Tie), std::invalid_argument);
}

TEST_CASE("net equals returned minus total stakes, for many mixes") {
    for (std::int64_t p : {0, 1, 100, 2599})
        for (std::int64_t bk : {0, 19, 500, 10000})
            for (std::int64_t t : {0, 21, 2500})
                for (Outcome o : {Outcome::Player, Outcome::Banker, Outcome::Tie}) {
                    const Bets b{p, bk, t};
                    const Settlement s = settle(b, o);
                    CHECK(s.netCents == s.returnedCents - b.total());
                    CHECK(s.returnedCents ==
                          s.at(BetSpot::Player).returnedCents + s.at(BetSpot::Banker).returnedCents +
                              s.at(BetSpot::Tie).returnedCents);
                }
}
