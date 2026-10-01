# CONTEXT — Baccarat (SDL3 + C++)

## Glossary
- **Punto Banco**: Fixed-rules baccarat. Player/Banker/Tie bets only.
- **Third-card rules**: The mandatory drawing table deciding whether Player/Banker draw a third card.
- **Commission**: 5% charged on winning Banker bets.
- **History strip**: Simple recent-results display (not bead-plate roads).

## Decisions
- Full Punto Banco ruleset with correct third-card table; no side bets in v1.
- Stack: SDL3 + SDL_ttf3 + SDL_mixer3 via CMake FetchContent.
