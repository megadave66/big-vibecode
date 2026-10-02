#pragma once
// One coup of Punto Banco: deal, apply the third-card rules, decide the outcome.

#include <vector>

#include "baccarat/rules/card.hpp"

namespace bac::rules {

class Shoe;

enum class Outcome { Player, Banker, Tie };
enum class Side { Player, Banker };

struct DealStep {
    Side side;
    Card card;
    int index;  // position in that side's hand: 0, 1 or 2
};

struct RoundResult {
    std::vector<Card> player;
    std::vector<Card> banker;
    int playerTotal = 0;
    int bankerTotal = 0;
    Outcome outcome = Outcome::Tie;
    bool playerNatural = false;
    bool bankerNatural = false;
    bool natural = false;          // either side had a natural
    std::vector<DealStep> deals;   // exact deal order, for animation
};

Outcome outcomeFor(int playerTotal, int bankerTotal);

// Deal order: P1, B1, P2, B2, then player third (if drawn), then banker third
// (if drawn). Does not reshuffle: call shoe.reshuffleIfNeeded() before.
RoundResult playRound(Shoe& shoe);

}  // namespace bac::rules
