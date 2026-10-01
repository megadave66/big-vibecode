# CONTEXT — 3D Roulette (Godot)

## Glossary
- **Wheel**: American roulette wheel — 38 pockets (1–36, 0, 00). Visually custom-built; everything else default casino styling.
- **Bet layout**: Standard American table bets, including the five-number bet (0, 00, 1, 2, 3 — pays 6:1).
- **Bankroll**: Session-only chip balance; no persistence beyond a session.
- **Chip**: Denomination token (1/5/25/100).

## Decisions
- ADR-worthy: **American wheel** (user chose it over the standard European default; affects pocket count, house edge, and adds the five-number bet).
- Everything besides the wheel follows default casino conventions.
