#pragma once
// Punto Banco third-card rules. All totals are 0..9; out-of-range input throws
// std::invalid_argument.

#include <optional>

namespace bac::rules {

// Player draws a third card on 0-5, stands on 6-7.
// Nobody draws if either side has a natural (8/9).
bool playerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal);

// Banker third-card table. playerThirdCardPoints is the point value (0..9) of
// the player's third card, or nullopt if the player stood.
//  - Either side natural: no draw.
//  - Player stood:   banker draws on 0-5, stands on 6-7.
//  - Player drew P:  0-2 draw; 3 draws unless P==8; 4 draws if P in 2..7;
//                    5 draws if P in 4..7; 6 draws if P in 6..7; 7 stands.
bool bankerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal,
                      std::optional<int> playerThirdCardPoints);

}  // namespace bac::rules
