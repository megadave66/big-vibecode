#pragma once
// bac::rules — Punto Banco rules engine. Pure C++20, no SDL, no mutable globals.
//
// Public API (one include gives everything):
//
// constants.hpp
//   struct Payout { int num; int den; };
//   kNumDecks=8, kCardsPerDeck=52, kPlayerPayout{1,1}, kBankerPayout{1,1},
//   kTiePayout{8,1}, kBankerCommissionPercent=5, kStartingBankrollCents=100000,
//   kChipValuesCents{100,500,2500,10000}, kCutCardRemaining=52,
//   kBurnFaceValue=10, kMaxCardsPerRound=6
//
// card.hpp
//   enum class Rank { Ace=1 .. King=13 };  enum class Suit { Clubs, Diamonds, Hearts, Spades };
//   struct Card { Rank rank; Suit suit; };   kAllRanks, kAllSuits
//   constexpr int  cardPoints(Card);                    // A=1, 2-9, 10/J/Q/K=0
//   constexpr int  handTotal(std::span<const Card>);    // sum mod 10
//   constexpr bool isNatural(int twoCardTotal);         // 8 or 9
//   std::string rankToString(Rank), suitToString(Suit), toString(Card);  // "AS","10H","KD"
//
// draw_rules.hpp
//   bool playerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal);
//   bool bankerShouldDraw(int playerTwoCardTotal, int bankerTwoCardTotal,
//                         std::optional<int> playerThirdCardPoints);
//
// shoe.hpp
//   enum class Burn { On, Off };   int burnCountFor(Card flipped);
//   class Shoe {
//     explicit Shoe(uint64_t seed, int numDecks = kNumDecks, Burn = Burn::On);
//     static Shoe stacked(std::vector<Card>, size_t cutCardRemaining = 0);
//     Card draw();  size_t remaining() const;  size_t size() const;
//     bool needsReshuffle() const;  void reshuffle();  bool reshuffleIfNeeded();
//     const std::vector<Card>& burned() const;  size_t cutCardRemaining() const;
//   };
//
// round.hpp
//   enum class Outcome { Player, Banker, Tie };  enum class Side { Player, Banker };
//   struct DealStep { Side side; Card card; int index; };
//   struct RoundResult { player, banker, playerTotal, bankerTotal, outcome,
//                        playerNatural, bankerNatural, natural, deals };
//   Outcome outcomeFor(int playerTotal, int bankerTotal);
//   RoundResult playRound(Shoe&);   // P1,B1,P2,B2,[P3],[B3]
//
// settlement.hpp
//   enum class BetSpot { Player, Banker, Tie };  enum class BetResult { NoBet, Win, Lose, Push };
//   struct Bets { int64_t player, banker, tie; total(); at(BetSpot); };
//   struct SpotSettlement { spot, stake, result, commissionCents, returnedCents, netCents };
//   struct Settlement { returnedCents, netCents, commissionCents, spots[3]; at(BetSpot); };
//   int64_t commissionFor(int64_t bankerWinningsCents);   // floor(w*5/100)
//   int64_t payoutFor(BetSpot, int64_t stakeCents);        // net winnings on a win
//   std::string payoutLabel(BetSpot);                       // "1:1","1:1 (5% comm.)","8:1"
//   Settlement settle(const Bets&, Outcome);

#include "baccarat/rules/card.hpp"
#include "baccarat/rules/constants.hpp"
#include "baccarat/rules/draw_rules.hpp"
#include "baccarat/rules/round.hpp"
#include "baccarat/rules/settlement.hpp"
#include "baccarat/rules/shoe.hpp"
