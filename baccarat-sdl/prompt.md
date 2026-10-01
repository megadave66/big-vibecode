# PROMPT — Baccarat (SDL3 + C++)

You are building a complete, playable Punto Banco baccarat game in `~/Documents/ai-projects/big-vibecode/baccarat-sdl`.

## Project summary
A full-rules **Punto Banco** casino table game. Stack: **SDL3 + SDL_ttf3 + SDL_mixer3** via CMake FetchContent (self-contained build). Linux first, portable code. Unit tests with **doctest** on ALL logic; headless smoke check (run N frames with dummy video driver, exit 0).

## Domain decisions (authoritative)
- Bets: Player (1:1), Banker (1:1 minus **5% commission**), Tie (8:1). No side bets in v1.
- The **third-card rules table** must be implemented exactly per Punto Banco: natural 8/9 stand; player draws on 0–5 (unless natural); banker draws per the standard table conditioned on the player's third card. This is the highest-risk section — test it exhaustively (every (banker total, player third card) combination).
- $1,000 starting bankroll; chips 1/5/25/100; click a betting spot to place the selected chip, click again to add / a remove control to take back; **click-when-ready** to deal (no timer).
- **History strip**: a simple horizontal recent-results display (P/B/T tiles) — not bead-plate roads.
- Card presentation: simple slide/flip animation.
- Session-only bankroll; no persistence required.
- Deck: 8 decks, reshuffle per casino convention (shoe penetration is fine at a fixed point); card values: A=1, 2–9 face, 10/J/Q/K=0; hand score = (sum mod 10).

## Asset policy
CC0 preferred, CC-BY with attribution, no NC-only licenses; log everything in `LICENSES.md` with source URLs. Delegate an asset-search sub-agent for: card faces/back, felt/table background, chip sprites (4 denominations), SFX (card slide, chip place, win/lose). Programmatic original art is an acceptable fallback — note it in handoff.md.

---

## Phase 0 — Divide & assign models
Pick model tiers per section: **easy → fastest model; hard logic → strongest reasoning model**. Sections:

1. **Scaffold** *(easy)* — CMake + FetchContent (SDL3, SDL_ttf3, SDL_mixer3), app loop, renderer init, scene/state skeleton.
2. **Rules engine** *(hard — strongest model)* — pure-logic module: deck/shoe, hand scoring, full third-card table, payout resolution with 5% commission. Zero SDL dependencies; fully doctest-covered including exhaustive third-card table tests and commission rounding tests.
3. **Table UI** *(medium)* — felt layout, betting spots with payout labels, chip placement/removal, card dealing animation (slide/flip), hand total badges, history strip.
4. **Game flow + audio** *(medium)* — state machine (betting → dealing → resolution → payout → next round), insufficient-funds handling (all-in / can't cover a bet), SFX wiring, table-min... none (no table minimum), polish pass.

## Phase 1 — Delegate to sub-agents
For EACH section:
- Spawn one **implementer sub-agent** with its spec, shared conventions, and test obligations.
- Spawn one **reviewer sub-agent** that checks the implementer's report against the section's **written test script**: in-repo doctest suites + `TESTS.md` checklist with exact verification commands (`cmake --build build`, `ctest`, smoke test, and for UI sections an interactive checklist the reviewer walks through with screenshots).
- Reviewer loops with the implementer until green; must attach real command output.

## Phase 2 — Integration & handoff
- Integrate: single source of truth for rules constants and payouts; UI consumes the rules engine — never reimplements it.
- Full verification: clean build, `ctest` green, smoke test green, TESTS.md fully ticked.
- Write **`handoff.md`**: what exists, how to build/run/test, architecture (rules engine is the deep module — document its API), known gaps. State + resume instructions only, no v2 ideas.