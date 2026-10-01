#include <doctest/doctest.h>

#include <vector>

#include "baccarat/rules/rules.hpp"

using namespace bac::rules;

namespace {
Card c(Rank r, Suit s = Suit::Spades) { return Card{r, s}; }
}  // namespace

TEST_CASE("cardPoints for all 13 ranks") {
    const int expected[13] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0};
    for (int i = 0; i < 13; ++i) {
        CAPTURE(i);
        for (Suit s : kAllSuits) CHECK(cardPoints(Card{kAllRanks[i], s}) == expected[i]);
    }
    static_assert(cardPoints(Card{Rank::Nine, Suit::Hearts}) == 9);
    static_assert(cardPoints(Card{Rank::King, Suit::Hearts}) == 0);
}

TEST_CASE("handTotal is sum mod 10") {
    CHECK(handTotal(std::vector<Card>{}) == 0);
    CHECK(handTotal(std::vector<Card>{c(Rank::Ace)}) == 1);
    CHECK(handTotal(std::vector<Card>{c(Rank::Five), c(Rank::Four)}) == 9);
    CHECK(handTotal(std::vector<Card>{c(Rank::Five), c(Rank::Five)}) == 0);
    CHECK(handTotal(std::vector<Card>{c(Rank::Seven), c(Rank::Eight)}) == 5);
    CHECK(handTotal(std::vector<Card>{c(Rank::King), c(Rank::Queen)}) == 0);
    CHECK(handTotal(std::vector<Card>{c(Rank::Ten), c(Rank::Eight)}) == 8);
    CHECK(handTotal(std::vector<Card>{c(Rank::Nine), c(Rank::Nine), c(Rank::Nine)}) == 7);
    CHECK(handTotal(std::vector<Card>{c(Rank::Jack), c(Rank::Ace), c(Rank::Nine)}) == 0);
    CHECK(handTotal(std::vector<Card>{c(Rank::Six), c(Rank::Seven), c(Rank::Eight)}) == 1);
    // Every two-card point pair.
    for (int a = 0; a < 13; ++a)
        for (int b = 0; b < 13; ++b) {
            const std::vector<Card> h{c(kAllRanks[a]), c(kAllRanks[b], Suit::Hearts)};
            const int pa = a < 9 ? a + 1 : 0;
            const int pb = b < 9 ? b + 1 : 0;
            CHECK(handTotal(h) == (pa + pb) % 10);
        }
}

TEST_CASE("isNatural only for 8 and 9") {
    for (int t = 0; t <= 9; ++t) CHECK(isNatural(t) == (t == 8 || t == 9));
}

TEST_CASE("toString formats rank then suit") {
    CHECK(toString(Card{Rank::Ace, Suit::Spades}) == "AS");
    CHECK(toString(Card{Rank::Ten, Suit::Hearts}) == "10H");
    CHECK(toString(Card{Rank::King, Suit::Diamonds}) == "KD");
    CHECK(toString(Card{Rank::Queen, Suit::Clubs}) == "QC");
    CHECK(toString(Card{Rank::Jack, Suit::Spades}) == "JS");
    CHECK(toString(Card{Rank::Two, Suit::Clubs}) == "2C");
    CHECK(toString(Card{Rank::Nine, Suit::Diamonds}) == "9D");
}
