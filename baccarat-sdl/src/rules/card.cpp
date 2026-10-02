#include "baccarat/rules/card.hpp"

namespace bac::rules {

std::string rankToString(Rank r) {
    switch (r) {
        case Rank::Ace: return "A";
        case Rank::Jack: return "J";
        case Rank::Queen: return "Q";
        case Rank::King: return "K";
        default: return std::to_string(static_cast<int>(r));
    }
}

std::string suitToString(Suit s) {
    switch (s) {
        case Suit::Clubs: return "C";
        case Suit::Diamonds: return "D";
        case Suit::Hearts: return "H";
        case Suit::Spades: return "S";
    }
    return "?";
}

std::string toString(Card c) { return rankToString(c.rank) + suitToString(c.suit); }

}  // namespace bac::rules
