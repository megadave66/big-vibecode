#include "baccarat/rules/draw_rules.hpp"

#include <stdexcept>

#include "baccarat/rules/card.hpp"

namespace bac::rules {
namespace {

void requireTotal(int v, const char* what) {
    if (v < 0 || v > 9) throw std::invalid_argument(what);
}

}  // namespace

bool playerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal) {
    requireTotal(playerTwoCardTotal, "player total out of range 0..9");
    requireTotal(bankerTwoCardTotal, "banker total out of range 0..9");
    if (isNatural(playerTwoCardTotal) || isNatural(bankerTwoCardTotal)) return false;
    return playerTwoCardTotal <= 5;
}

bool bankerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal,
                      std::optional<int> playerThirdCardPoints) {
    requireTotal(playerTwoCardTotal, "player total out of range 0..9");
    requireTotal(bankerTwoCardTotal, "banker total out of range 0..9");
    if (playerThirdCardPoints) requireTotal(*playerThirdCardPoints, "third card points out of range 0..9");

    if (isNatural(playerTwoCardTotal) || isNatural(bankerTwoCardTotal)) return false;

    const int b = bankerTwoCardTotal;
    if (!playerThirdCardPoints) return b <= 5;  // player stood

    const int p = *playerThirdCardPoints;
    switch (b) {
        case 0:
        case 1:
        case 2: return true;
        case 3: return p != 8;
        case 4: return p >= 2 && p <= 7;
        case 5: return p >= 4 && p <= 7;
        case 6: return p == 6 || p == 7;
        default: return false;  // 7
    }
}

}  // namespace bac::rules
