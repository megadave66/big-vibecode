#pragma once
// Cards, card points and hand totals.

#include <array>
#include <span>
#include <string>

namespace bac::rules {

enum class Rank : int {
    Ace = 1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King
};

enum class Suit : int { Clubs, Diamonds, Hearts, Spades };

inline constexpr std::array<Rank, 13> kAllRanks{
    Rank::Ace,  Rank::Two,  Rank::Three, Rank::Four, Rank::Five,  Rank::Six, Rank::Seven,
    Rank::Eight, Rank::Nine, Rank::Ten,  Rank::Jack, Rank::Queen, Rank::King};

inline constexpr std::array<Suit, 4> kAllSuits{Suit::Clubs, Suit::Diamonds, Suit::Hearts,
                                               Suit::Spades};

struct Card {
    Rank rank;
    Suit suit;
    friend constexpr bool operator==(const Card&, const Card&) = default;
};

// Baccarat point value: A=1, 2-9 face value, 10/J/Q/K=0.
constexpr int cardPoints(Card c) {
    const int r = static_cast<int>(c.rank);
    return r >= 10 ? 0 : r;
}

// Hand score = sum of card points mod 10.
constexpr int handTotal(std::span<const Card> cards) {
    int sum = 0;
    for (const Card& c : cards) sum += cardPoints(c);
    return sum % 10;
}

// A two-card total of 8 or 9.
constexpr bool isNatural(int twoCardTotal) { return twoCardTotal == 8 || twoCardTotal == 9; }

// "A","2".."10","J","Q","K" / "C","D","H","S"; toString gives e.g. "AS", "10H", "KD".
std::string rankToString(Rank r);
std::string suitToString(Suit s);
std::string toString(Card c);

}  // namespace bac::rules
