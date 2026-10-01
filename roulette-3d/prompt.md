# PROMPT — 3D Roulette (Godot 4)

You are building a complete, playable 3D casino roulette game in `~/Documents/ai-projects/big-vibecode/roulette-3d` with **Godot 4.x (latest stable) and GDScript**.

## Project summary
An American-wheel roulette table in 3D with **physically simulated ball physics**. Everything except the wheel follows default casino conventions. Unit tests with **gdUnit4** (headless-capable); reproducible builds via a headless export script.

## Domain decisions (authoritative)
- **American wheel: 38 pockets** (1–36, single 0, double 00) — user's explicit choice over European.
- **Ball physics: real Godot rigid body.** Launched tangentially opposite the wheel's spin, decelerates, interacts with frets and diamond deflectors, settles into whichever pocket it lands in. **NO predetermined outcome, no rigged results.** The wheel spins at a fixed/gradually-varied speed; ball launch velocity gets slight randomized variation for realism.
- **Bet layout**: full standard American table — straights (35:1), splits (17:1), streets (11:1), corners (8:1), five-number bet 0/00/1/2/3 (6:1), dozens/columns (2:1), red/black, even/odd, low/high (1:1). Zero/00 splits and trio bets handled correctly.
- $1,000 starting bankroll (session-only). Chips: 1/5/25/100 denominations, selectable. **Left-click a bet spot to place the selected chip (stacks), right-click to remove.**
- Flow: betting phase → launch ball → **locked-bets state** ("no more bets") → ball settles → winning number highlighted → chips resolved (losers swept, winners paid with matched chips) → bankroll updated → next round. No table minimum.
- Include: settings screen (sound volume) and how-to-play screen.
- Table + wheel visuals: built from Godot primitives/materials (procedural geometry is fine); the wheel pocket ring must be authored to real American wheel number order.

## Asset policy
CC0 preferred, CC-BY with attribution, no NC-only licenses; log in `LICENSES.md`. Delegate an asset-search sub-agent for: chip textures, felt/material textures, ambient casino SFX, ball/wheel click sounds, win chime. Procedural fallback is acceptable — note it in handoff.md.

---

## Phase 0 — Divide & assign models
Pick model tiers per section: **easy → fastest model; hard → strongest reasoning model**. Sections:

1. **Scaffold + 3D scene** *(easy-medium)* — Godot project structure, table/wheel/chip scenes, camera, lighting, input plumbing.
2. **Wheel physics** *(hard — strongest model)* — wheel spinner, ball rigid body, frets/diamond colliders, deceleration tuning, pocket detection at rest, settle determinism across repeated runs.
3. **Bet layout + chip UI** *(medium-hard)* — full American bet-spot map (all bet types + neighbors), chip placement/stacking/removal, bet-total accounting.
4. **Resolution & game flow** *(hard)* — payout table implementation for every bet type (doctest-equivalent gdUnit4 tests on all resolutions), locked-bets state machine, winning-number flow, bankroll math incl. five-number bet edge cases.
5. **Assets, audio & polish** *(easy)* — asset sourcing, SFX (ball rattle, clicks, win), settings + how-to-play, visual pass.

## Phase 1 — Delegate to sub-agents
For EACH section:
- Spawn one **implementer sub-agent** with its spec, shared conventions, and test obligations.
- Spawn one **reviewer sub-agent** that verifies against the section's **written test script**: gdUnit4 suites (payouts, bankroll, pocket ordering, physics settle-invariants like "ball always ends in exactly one pocket") + `TESTS.md` checklist with exact commands (headless Godot test run, export script) and interactive visual checks the reviewer performs.
- Reviewer loops with implementer until green; real command output required.

## Phase 2 — Integration & handoff
- Integrate: single payout constants source shared by UI tooltips and resolution logic; the wheel section's pocket detection feeds resolution directly.
- Verify: gdUnit4 suite green headless, export script produces a runnable build, TESTS.md ticked.
- Write **`handoff.md`**: what exists, how to open/build/export/test, architecture (physics tunables, bet-spot map data layout), known gaps. State + resume only — no v2 ideas.