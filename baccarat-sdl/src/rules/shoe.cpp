#include "baccarat/rules/shoe.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace bac::rules {
namespace {

// Uniform integer in [0, bound) by rejection sampling. Fully specified, so it
// gives the same sequence on every standard library (unlike
// std::uniform_int_distribution).
std::uint64_t uniformBelow(std::mt19937_64& rng, std::uint64_t bound) {
    // 2^64 mod bound: values below this would bias the modulo.
    const std::uint64_t threshold = (0 - bound) % bound;
    for (;;) {
        const std::uint64_t r = rng();
        if (r >= threshold) return r % bound;
    }
}

}  // namespace

int burnCountFor(Card flipped) {
    const int r = static_cast<int>(flipped.rank);
    return r >= 10 ? kBurnFaceValue : r;
}

Shoe::Shoe(std::uint64_t seed, int numDecks, Burn burn) : rng_(seed), burn_(burn) {
    if (numDecks < 1) throw std::invalid_argument("numDecks must be >= 1");
    cards_.reserve(static_cast<std::size_t>(numDecks) * kCardsPerDeck);
    for (int d = 0; d < numDecks; ++d)
        for (Suit s : kAllSuits)
            for (Rank r : kAllRanks) cards_.push_back(Card{r, s});
    shuffleAndBurn();
}

Shoe Shoe::stacked(std::vector<Card> cards, std::size_t cutCardRemaining) {
    Shoe shoe;
    shoe.cards_ = std::move(cards);
    shoe.cutCardRemaining_ = cutCardRemaining;
    shoe.shuffles_ = false;
    shoe.burn_ = Burn::Off;
    return shoe;
}

Card Shoe::draw() {
    if (next_ >= cards_.size()) throw std::out_of_range("shoe is empty");
    return cards_[next_++];
}

void Shoe::reshuffle() {
    next_ = 0;
    burned_.clear();
    if (shuffles_) shuffleAndBurn();
}

bool Shoe::reshuffleIfNeeded() {
    if (!needsReshuffle()) return false;
    reshuffle();
    return true;
}

void Shoe::shuffleAndBurn() {
    next_ = 0;
    burned_.clear();
    // Fisher-Yates, back to front.
    for (std::size_t i = cards_.size(); i > 1; --i) {
        const auto j = static_cast<std::size_t>(uniformBelow(rng_, i));
        std::swap(cards_[i - 1], cards_[j]);
    }
    if (burn_ == Burn::On && !cards_.empty()) {
        const Card flipped = draw();
        burned_.push_back(flipped);
        const int n = burnCountFor(flipped);
        for (int k = 0; k < n && remaining() > 0; ++k) burned_.push_back(draw());
    }
}

}  // namespace bac::rules
