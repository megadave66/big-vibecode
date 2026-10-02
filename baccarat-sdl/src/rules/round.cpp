#include "baccarat/rules/round.hpp"

#include <optional>

#include "baccarat/rules/constants.hpp"
#include "baccarat/rules/draw_rules.hpp"
#include "baccarat/rules/shoe.hpp"

namespace bac::rules {

Outcome outcomeFor(int playerTotal, int bankerTotal) {
    if (playerTotal > bankerTotal) return Outcome::Player;
    if (bankerTotal > playerTotal) return Outcome::Banker;
    return Outcome::Tie;
}

RoundResult playRound(Shoe& shoe) {
    RoundResult r;
    r.player.reserve(3);
    r.banker.reserve(3);
    r.deals.reserve(kMaxCardsPerRound);

    auto deal = [&](Side side) {
        auto& hand = side == Side::Player ? r.player : r.banker;
        const Card c = shoe.draw();
        r.deals.push_back(DealStep{side, c, static_cast<int>(hand.size())});
        hand.push_back(c);
        return c;
    };

    deal(Side::Player);
    deal(Side::Banker);
    deal(Side::Player);
    deal(Side::Banker);

    const int p2 = handTotal(r.player);
    const int b2 = handTotal(r.banker);
    r.playerNatural = isNatural(p2);
    r.bankerNatural = isNatural(b2);
    r.natural = r.playerNatural || r.bankerNatural;

    std::optional<int> playerThird;
    if (playerShouldDraw(p2, b2)) playerThird = cardPoints(deal(Side::Player));
    if (bankerShouldDraw(p2, b2, playerThird)) deal(Side::Banker);

    r.playerTotal = handTotal(r.player);
    r.bankerTotal = handTotal(r.banker);
    r.outcome = outcomeFor(r.playerTotal, r.bankerTotal);
    return r;
}

}  // namespace bac::rules
