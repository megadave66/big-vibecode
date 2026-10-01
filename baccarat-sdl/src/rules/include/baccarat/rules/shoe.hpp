#pragma once
// The shoe: N decks, deterministic shuffle, cut card and burn.

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "baccarat/rules/card.hpp"
#include "baccarat/rules/constants.hpp"

namespace bac::rules {

enum class Burn { On, Off };

// Cards burned for a given flipped card: Ace=1, 2-9 face, 10/J/Q/K = kBurnFaceValue.
int burnCountFor(Card flipped);

class Shoe {
public:
    // Builds numDecks decks, shuffles with std::mt19937_64(seed) and our own
    // Fisher-Yates (identical on every standard library), then burns if
    // burn == Burn::On.
    explicit Shoe(std::uint64_t seed, int numDecks = kNumDecks, Burn burn = Burn::On);

    // Test factory: draws `cards` in the given order. No shuffle, no burn.
    // reshuffle() restores the same order. Cut point defaults to 0 (never
    // needs reshuffle until empty).
    static Shoe stacked(std::vector<Card> cards, std::size_t cutCardRemaining = 0);

    // Draws the next card. Throws std::out_of_range if the shoe is empty.
    Card draw();

    std::size_t remaining() const { return cards_.size() - next_; }
    std::size_t size() const { return cards_.size(); }  // full shoe size

    // True when fewer than the cut-card count remain. Check at round start.
    bool needsReshuffle() const { return remaining() < cutCardRemaining_; }

    // Gathers all cards and reshuffles (continuing the same RNG stream, so
    // runs stay deterministic per seed). Burns again if burn is on.
    void reshuffle();

    // Reshuffles only if needsReshuffle(). Returns true if it reshuffled.
    bool reshuffleIfNeeded();

    // Cards burned after the latest shuffle: the flipped card first, then the
    // burned cards. Empty when burn is off or for stacked shoes.
    const std::vector<Card>& burned() const { return burned_; }

    std::size_t cutCardRemaining() const { return cutCardRemaining_; }

private:
    Shoe() = default;
    void shuffleAndBurn();

    std::vector<Card> cards_;
    std::size_t next_ = 0;
    std::size_t cutCardRemaining_ = kCutCardRemaining;
    std::mt19937_64 rng_;
    bool shuffles_ = true;  // false for stacked shoes
    Burn burn_ = Burn::On;
    std::vector<Card> burned_;
};

}  // namespace bac::rules
