#include <doctest/doctest.h>

#include <map>
#include <random>
#include <string>
#include <stdexcept>
#include <vector>

#include "baccarat/rules/rules.hpp"

using namespace bac::rules;

namespace {

std::vector<Card> drawAll(Shoe& s) {
    std::vector<Card> out;
    while (s.remaining() > 0) out.push_back(s.draw());
    return out;
}

}  // namespace

TEST_CASE("8-deck shoe without burn has 416 cards, each rank/suit exactly 8 times") {
    Shoe s(42, kNumDecks, Burn::Off);
    CHECK(s.size() == 416);
    CHECK(s.remaining() == 416);
    CHECK(s.burned().empty());
    const auto cards = drawAll(s);
    REQUIRE(cards.size() == 416);
    std::map<std::pair<int, int>, int> counts;
    for (const Card& c : cards) ++counts[{static_cast<int>(c.rank), static_cast<int>(c.suit)}];
    CHECK(counts.size() == 52);
    for (Rank r : kAllRanks)
        for (Suit su : kAllSuits) {
            CAPTURE(toString(Card{r, su}));
            CHECK(counts[{static_cast<int>(r), static_cast<int>(su)}] == 8);
        }
    CHECK_THROWS_AS(s.draw(), std::out_of_range);
}

TEST_CASE("shoe is actually shuffled") {
    Shoe s(7, 1, Burn::Off);
    const auto cards = drawAll(s);
    int inPlace = 0;
    int i = 0;
    for (Suit su : kAllSuits)
        for (Rank r : kAllRanks) inPlace += (cards[static_cast<std::size_t>(i++)] == Card{r, su});
    CHECK(inPlace < 10);
}

TEST_CASE("same seed gives same order, different seed differs") {
    Shoe a(12345), b(12345), c(12346);
    const auto ca = drawAll(a), cb = drawAll(b), cc = drawAll(c);
    CHECK(ca == cb);
    CHECK(a.burned() == b.burned());
    CHECK(ca != cc);
}

TEST_CASE("shuffle is pinned: cross-library golden sequence") {
    // Our Fisher-Yates over mt19937_64 is fully specified, so this order is
    // the same on libc++, libstdc++ and MSVC. If this fails, saved seeds no
    // longer replay the same shoe.
    Shoe s(1, 1, Burn::Off);
    std::string first;
    for (int i = 0; i < 8; ++i) first += toString(s.draw()) + " ";
    MESSAGE("seed 1, 1 deck, first 8: " << first);
    CHECK(first == "6C 2C 6S 5S 9D JS 8D 10D ");
}

TEST_CASE("mt19937_64 conforms to the standard") {
    // [rand.predef]: the 10000th output of a default-constructed engine.
    std::mt19937_64 e;
    e.discard(9999);
    CHECK(e() == 9981545732273789042ULL);
}

TEST_CASE("needsReshuffle at the cut card") {
    Shoe s(99, kNumDecks, Burn::Off);
    CHECK_FALSE(s.needsReshuffle());
    CHECK(s.cutCardRemaining() == kCutCardRemaining);
    while (s.remaining() > kCutCardRemaining) s.draw();
    CHECK(s.remaining() == 52);
    CHECK_FALSE(s.needsReshuffle());  // exactly 52 left: still play
    s.draw();
    CHECK(s.remaining() == 51);
    CHECK(s.needsReshuffle());
    CHECK(s.reshuffleIfNeeded());
    CHECK(s.remaining() == 416);
    CHECK_FALSE(s.needsReshuffle());
    CHECK_FALSE(s.reshuffleIfNeeded());
}

TEST_CASE("reshuffle restores full count and keeps composition") {
    Shoe s(5, kNumDecks, Burn::Off);
    const auto before = drawAll(s);
    s.reshuffle();
    CHECK(s.remaining() == 416);
    const auto after = drawAll(s);
    CHECK(after != before);  // new shuffle, RNG stream continues
    std::map<std::pair<int, int>, int> counts;
    for (const Card& c : after) ++counts[{static_cast<int>(c.rank), static_cast<int>(c.suit)}];
    for (const auto& [k, v] : counts) CHECK(v == 8);
}

TEST_CASE("reshuffle is deterministic per seed") {
    Shoe a(77), b(77);
    drawAll(a);
    for (int i = 0; i < 10; ++i) b.draw();
    a.reshuffle();
    b.reshuffle();
    CHECK(drawAll(a) == drawAll(b));
}

TEST_CASE("burnCountFor: A=1, 2-9 face, 10/J/Q/K=10") {
    const int expected[13] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};
    for (int i = 0; i < 13; ++i) CHECK(burnCountFor(Card{kAllRanks[i], Suit::Clubs}) == expected[i]);
}

TEST_CASE("burn on new shoe: flip one, burn that many") {
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        CAPTURE(seed);
        Shoe burned(seed);  // Burn::On default
        Shoe plain(seed, kNumDecks, Burn::Off);
        REQUIRE_FALSE(burned.burned().empty());
        const Card flipped = burned.burned().front();
        const std::size_t n = static_cast<std::size_t>(burnCountFor(flipped));
        CHECK(burned.burned().size() == 1 + n);
        CHECK(burned.remaining() == 416 - 1 - n);
        // Same shuffle; the burned cards are exactly the top of the plain shoe.
        for (const Card& c : burned.burned()) CHECK(plain.draw() == c);
        CHECK(burned.draw() == plain.draw());
    }
}

TEST_CASE("burn happens again on reshuffle") {
    Shoe s(3);
    const auto firstBurn = s.burned();
    s.reshuffle();
    REQUIRE_FALSE(s.burned().empty());
    CHECK(s.remaining() == 416 - s.burned().size());
    CHECK(s.burned().size() == 1 + static_cast<std::size_t>(burnCountFor(s.burned().front())));
}

TEST_CASE("stacked shoe draws in order, no shuffle, no burn") {
    const std::vector<Card> cards{{Rank::Ace, Suit::Spades}, {Rank::Ten, Suit::Hearts},
                                  {Rank::King, Suit::Diamonds}};
    Shoe s = Shoe::stacked(cards);
    CHECK(s.size() == 3);
    CHECK(s.burned().empty());
    CHECK_FALSE(s.needsReshuffle());
    CHECK(s.draw() == cards[0]);
    CHECK(s.draw() == cards[1]);
    CHECK(s.draw() == cards[2]);
    CHECK_THROWS_AS(s.draw(), std::out_of_range);
    s.reshuffle();
    CHECK(s.remaining() == 3);
    CHECK(s.draw() == cards[0]);

    Shoe cut = Shoe::stacked(cards, 2);
    CHECK_FALSE(cut.needsReshuffle());
    cut.draw();
    CHECK_FALSE(cut.needsReshuffle());
    cut.draw();
    CHECK(cut.needsReshuffle());
}

TEST_CASE("invalid deck count throws") {
    CHECK_THROWS_AS(Shoe(1, 0), std::invalid_argument);
}
