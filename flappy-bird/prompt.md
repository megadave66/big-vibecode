# PROMPT — Flappy Bird Remake (SDL3 + C++)

You are building a complete, playable Flappy Bird–style game in `~/Documents/ai-projects/big-vibecode/flappy-bird`.

## Project summary
An inspired-by remake of Flappy Bird with **original assets** (no ripped sprites/music/audio). Stack: **SDL3 + SDL_ttf3 + SDL_mixer3**, CMake with FetchContent so the build is fully self-contained. Target Linux first; keep the code portable so Windows is possible later. No CI matrix needed for v1.

## Domain decisions (authoritative)
- Scope: get-ready screen → tap to flap → endless pipes with fair, randomized gap sizing → score → game-over panel with restart → persisted local best score (small file in the user config/cache dir, not the repo). Sound effects: flap, score, hit.
- NO medals, NO day/night cycle. Definition of done: **runs + playable + tests pass** — but the listed scope must be complete and feel correct (fixed timestep or delta-time physics, no frame-rate dependence).
- Unit-test all non-rendering logic (pipe gap generation bounds, collision box math, score increment, best-score persistence round-trip) with **doctest**.
- Ship a headless smoke check: build and run the game for N simulated frames without a display (SDL hint / dummy video driver) and exit 0.

## Asset policy
CC0/public-domain preferred; CC-BY acceptable with attribution; no NC-only licenses; every asset logged in `LICENSES.md` with source URL; nothing from the original Flappy Bird. Delegate an asset-search sub-agent to find: bird sprite/frames, pipe texture or flat-color art, background, flap/score/hit SFX. If suitable assets can't be found, generate simple original placeholder art programmatically and say so in handoff.md.

---

## Phase 0 — Divide & assign models
Split the project into these sections. For each, pick the appropriate model tier: **easy → your fastest model; hard logic/physics → your strongest reasoning model**. Sections:

1. **Scaffold** *(easy)* — CMake project, FetchContent for SDL3/SDL_ttf3/SDL_mixer3, main loop with fixed timestep, window/renderer init, exit handling.
2. **Bird physics + input** *(medium)* — gravity, flap impulse, rotation, screen bounds, death on ground/ceiling.
3. **Pipes, scoring & collision** *(medium)* — spawn cadence, fair gap sizing (bounded randomization), AABB collision, score increments.
4. **UI & state machine** *(easy-medium)* — get-ready screen, HUD score, game-over panel, restart, best-score persistence.
5. **Assets & audio** *(easy)* — source/find assets per policy, wire SFX and music, LICENSES.md.

## Phase 1 — Delegate to sub-agents
For EACH section:
- Spawn one **implementer sub-agent** with the section spec, the shared conventions (style, dirs, no drive-by refactors), and its test obligations.
- Spawn one **reviewer sub-agent** that verifies the implementer's report against the section's **written test script** (in-repo doctest cases + a `TESTS.md` checklist entry with the exact commands to run: `cmake --build build`, `ctest`, the smoke check).
- The reviewer loops with the implementer until every test is green and the checklist items are checked. Reviewer output must include the actual command output, not just "passed".

## Phase 2 — Integration & handoff
- Integrate all sections: one coherent state machine, single settings/config for constants (gravity, flap strength, gap size), no duplicate helpers.
- Run the full suite: build clean (no warnings), `ctest` green, smoke test green, playthrough checklist from TESTS.md ticked.
- Write **`handoff.md`** documenting: what exists (files + what each does), how to build/run/test, architecture notes, and a **known gaps** section. It is strictly documentation of state + resume instructions — no "v2 ideas".