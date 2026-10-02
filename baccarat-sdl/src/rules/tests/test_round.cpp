#include <doctest/doctest.h>

#include <vector>

#include "baccarat/rules/rules.hpp"

using namespace bac::rules;

namespace {

// Card with given point value. 0 cycles through 10/J/Q/K via `zeroVariant`.
Card pts(int p, Suit s = Suit::Spades, int zeroVariant = 0) {
    if (p == 0) {
        static constexpr Rank zeros[4] = {Rank::Ten, Rank::Jack, Rank::Queen, Rank::King};
        return Card{zeros[zeroVariant % 4], s};
    }
    return Card{static_cast<Rank>(p), s};
}

// Stack order is the deal order: P1, B1, P2, B2, then extras.
Shoe stack(int p1, int b1, int p2, int b2, std::vector<int> extra = {}) {
    std::vector<Card> cards{pts(p1, Suit::Spades), pts(b1, Suit::Hearts), pts(p2, Suit::Clubs),
                            pts(b2, Suit::Diamonds)};
    for (int e : extra) cards.push_back(pts(e, Suit::Spades, 1));
    return Shoe::stacked(cards);
}

void checkDealOrder(const RoundResult& r) {
    REQUIRE(r.deals.size() == r.player.size() + r.banker.size());
    REQUIRE(r.deals.size() >= 4);
    CHECK(r.deals[0].side == Side::Player);
    CHECK(r.deals[0].index == 0);
    CHECK(r.deals[1].side == Side::Banker);
    CHECK(r.deals[1].index == 0);
    CHECK(r.deals[2].side == Side::Player);
    CHECK(r.deals[2].index == 1);
    CHECK(r.deals[3].side == Side::Banker);
    CHECK(r.deals[3].index == 1);
    std::size_t k = 4;
    if (r.player.size() == 3) {
        CHECK(r.deals[k].side == Side::Player);
        CHECK(r.deals[k].index == 2);
        CHECK(r.deals[k].card == r.player[2]);
        ++k;
    }
    if (r.banker.size() == 3) {
        CHECK(r.deals[k].side == Side::Banker);
        CHECK(r.deals[k].index == 2);
        CHECK(r.deals[k].card == r.banker[2]);
        ++k;
    }
    CHECK(k == r.deals.size());
    for (const DealStep& d : r.deals) {
        const auto& hand = d.side == Side::Player ? r.player : r.banker;
        CHECK(hand[static_cast<std::size_t>(d.index)] == d.card);
    }
}

}  // namespace

TEST_CASE("outcomeFor") {
    CHECK(outcomeFor(9, 1) == Outcome::Player);
    CHECK(outcomeFor(1, 9) == Outcome::Banker);
    CHECK(outcomeFor(5, 5) == Outcome::Tie);
    CHECK(outcomeFor(0, 0) == Outcome::Tie);
}

TEST_CASE("naturals: nobody draws") {
    SUBCASE("player natural 9 beats banker 7") {
        Shoe s = stack(4, 3, 5, 4, {1, 1});
        const auto r = playRound(s);
        CHECK(r.player.size() == 2);
        CHECK(r.banker.size() == 2);
        CHECK(r.playerTotal == 9);
        CHECK(r.bankerTotal == 7);
        CHECK(r.playerNatural);
        CHECK_FALSE(r.bankerNatural);
        CHECK(r.natural);
        CHECK(r.outcome == Outcome::Player);
        CHECK(s.remaining() == 2);
        checkDealOrder(r);
    }
    SUBCASE("player natural 8, banker 0 does not draw") {
        Shoe s = stack(8, 0, 0, 0, {1, 1});
        const auto r = playRound(s);
        CHECK(r.banker.size() == 2);
        CHECK(r.playerTotal == 8);
        CHECK(r.bankerTotal == 0);
        CHECK(r.outcome == Outcome::Player);
        CHECK(s.remaining() == 2);
    }
    SUBCASE("banker natural 8, player 0 does not draw") {
        Shoe s = stack(0, 4, 0, 4, {1, 1});
        const auto r = playRound(s);
        CHECK(r.player.size() == 2);
        CHECK(r.banker.size() == 2);
        CHECK(r.bankerTotal == 8);
        CHECK(r.bankerNatural);
        CHECK_FALSE(r.playerNatural);
        CHECK(r.outcome == Outcome::Banker);
        checkDealOrder(r);
    }
    SUBCASE("banker natural 9 beats player natural 8") {
        Shoe s = stack(3, 9, 5, 0);
        const auto r = playRound(s);
        CHECK(r.playerTotal == 8);
        CHECK(r.bankerTotal == 9);
        CHECK(r.playerNatural);
        CHECK(r.bankerNatural);
        CHECK(r.outcome == Outcome::Banker);
    }
    SUBCASE("both natural 8: tie") {
        Shoe s = stack(8, 4, 0, 4);
        const auto r = playRound(s);
        CHECK(r.playerTotal == 8);
        CHECK(r.bankerTotal == 8);
        CHECK(r.outcome == Outcome::Tie);
        CHECK(r.natural);
        CHECK(r.deals.size() == 4);
    }
}

TEST_CASE("player stands on 6/7") {
    SUBCASE("player 6, banker 5 draws (player stood)") {
        Shoe s = stack(3, 2, 3, 3, {4});
        const auto r = playRound(s);
        CHECK(r.player.size() == 2);
        CHECK(r.banker.size() == 3);
        CHECK(r.playerTotal == 6);
        CHECK(r.bankerTotal == 9);
        CHECK(r.outcome == Outcome::Banker);
        CHECK_FALSE(r.natural);
        checkDealOrder(r);
    }
    SUBCASE("player 7, banker 6 stands") {
        Shoe s = stack(7, 6, 0, 0, {1});
        const auto r = playRound(s);
        CHECK(r.player.size() == 2);
        CHECK(r.banker.size() == 2);
        CHECK(r.outcome == Outcome::Player);
        CHECK(s.remaining() == 1);
    }
    SUBCASE("player 6, banker 6: tie, nobody draws") {
        Shoe s = stack(6, 6, 0, 0, {1});
        const auto r = playRound(s);
        CHECK(r.deals.size() == 4);
        CHECK(r.outcome == Outcome::Tie);
    }
    SUBCASE("player 7, banker 7 stands") {
        Shoe s = stack(7, 7, 0, 0, {1});
        const auto r = playRound(s);
        CHECK(r.deals.size() == 4);
        CHECK(r.outcome == Outcome::Tie);
    }
}

TEST_CASE("banker draw branches after player third card") {
    struct Case { int bankerTotal; int third; bool bankerDraws; };
    const Case cases[] = {
        {0, 9, true}, {1, 0, true}, {2, 8, true},
        {3, 8, false}, {3, 9, true}, {3, 0, true},
        {4, 1, false}, {4, 2, true}, {4, 7, true}, {4, 8, false},
        {5, 3, false}, {5, 4, true}, {5, 7, true}, {5, 8, false},
        {6, 5, false}, {6, 6, true}, {6, 7, true}, {6, 8, false},
        {7, 6, false}, {7, 7, false},
    };
    for (const Case& c : cases) {
        CAPTURE(c.bankerTotal);
        CAPTURE(c.third);
        // Player 0+3 = 3 draws. Banker total via (c.bankerTotal, 0).
        Shoe s = stack(0, c.bankerTotal, 3, 0, {c.third, 5});
        const auto r = playRound(s);
        CHECK(r.player.size() == 3);
        CHECK(cardPoints(r.player[2]) == c.third);
        CHECK(r.banker.size() == (c.bankerDraws ? 3u : 2u));
        CHECK(r.playerTotal == (3 + c.third) % 10);
        CHECK(r.bankerTotal == (c.bankerTotal + (c.bankerDraws ? 5 : 0)) % 10);
        CHECK(s.remaining() == (c.bankerDraws ? 0u : 1u));
        checkDealOrder(r);
    }
}

// ---- Brute force against a separately written reference ------------------

namespace ref {

// Reference written from the rules card, phrased differently on purpose.
struct Expected { int playerCards; int bankerCards; int pTotal; int bTotal; Outcome out; };

Expected play(int p1, int b1, int p2, int b2, int x1, int x2) {
    int p = (p1 + p2) % 10;
    int b = (b1 + b2) % 10;
    int pc = 2, bc = 2;
    const bool nat = p >= 8 || b >= 8;
    if (!nat) {
        int nextCard = x1;
        bool playerDrew = false;
        int third = -1;
        if (p < 6) {
            third = nextCard;
            p = (p + third) % 10;
            pc = 3;
            playerDrew = true;
            nextCard = x2;
        }
        bool bankerDraws;
        if (!playerDrew) {
            bankerDraws = b < 6;
        } else {
            static const bool table[8][10] = {
                // third: 0  1  2  3  4  5  6  7  8  9
                /*0*/ {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                /*1*/ {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                /*2*/ {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                /*3*/ {1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
                /*4*/ {0, 0, 1, 1, 1, 1, 1, 1, 0, 0},
                /*5*/ {0, 0, 0, 0, 1, 1, 1, 1, 0, 0},
                /*6*/ {0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
                /*7*/ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            };
            bankerDraws = table[b][third];
        }
        if (bankerDraws) {
            b = (b + nextCard) % 10;
            bc = 3;
        }
    }
    const Outcome o = p > b ? Outcome::Player : (b > p ? Outcome::Banker : Outcome::Tie);
    return {pc, bc, p, b, o};
}

}  // namespace ref

TEST_CASE("brute force: every 2-card start x every third card vs reference") {
    long rounds = 0, mismatches = 0;
    for (int p1 = 0; p1 <= 9; ++p1)
    for (int b1 = 0; b1 <= 9; ++b1)
    for (int p2 = 0; p2 <= 9; ++p2)
    for (int b2 = 0; b2 <= 9; ++b2)
    for (int x1 = 0; x1 <= 9; ++x1)
    for (int x2 = 0; x2 <= 9; ++x2) {
        const int zv = (p1 + b1 + x1) % 4;  // vary which 0-point rank is used
        std::vector<Card> cards{pts(p1, Suit::Spades, zv), pts(b1, Suit::Hearts, zv + 1),
                                pts(p2, Suit::Clubs, zv + 2), pts(b2, Suit::Diamonds, zv + 3),
                                pts(x1, Suit::Spades, zv), pts(x2, Suit::Hearts, zv)};
        Shoe s = Shoe::stacked(cards);
        const RoundResult r = playRound(s);
        const ref::Expected e = ref::play(p1, b1, p2, b2, x1, x2);
        ++rounds;
        const bool ok = static_cast<int>(r.player.size()) == e.playerCards &&
                        static_cast<int>(r.banker.size()) == e.bankerCards &&
                        r.playerTotal == e.pTotal && r.bankerTotal == e.bTotal &&
                        r.outcome == e.out &&
                        s.remaining() == 6u - static_cast<unsigned>(e.playerCards + e.bankerCards) &&
                        r.deals.size() == static_cast<std::size_t>(e.playerCards + e.bankerCards) &&
                        r.natural == ((p1 + p2) % 10 >= 8 || (b1 + b2) % 10 >= 8);
        if (!ok) {
            ++mismatches;
            if (mismatches <= 5) {
                INFO("p1=" << p1 << " b1=" << b1 << " p2=" << p2 << " b2=" << b2 << " x1=" << x1
                           << " x2=" << x2);
                CHECK(ok);
            }
        }
        if (rounds % 997 == 0) checkDealOrder(r);  // spot-check deal order structure
    }
    CHECK(rounds == 1000000);
    CHECK(mismatches == 0);
}

TEST_CASE("full shoe play-through stays consistent") {
    Shoe s(2024);
    int rounds = 0;
    while (!s.needsReshuffle()) {
        const auto before = s.remaining();
        const auto r = playRound(s);
        CHECK(before - s.remaining() == r.deals.size());
        CHECK(r.playerTotal == handTotal(r.player));
        CHECK(r.bankerTotal == handTotal(r.banker));
        CHECK(r.outcome == outcomeFor(r.playerTotal, r.bankerTotal));
        ++rounds;
    }
    CHECK(rounds > 50);
    CHECK(s.remaining() >= static_cast<std::size_t>(kCutCardRemaining - kMaxCardsPerRound));
}
