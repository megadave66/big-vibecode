# PROMPT — Geometry Dash Remake (SDL3 + C++)

You are building a complete, playable Geometry Dash–inspired game with **10 levels** in `~/Documents/ai-projects/big-vibecode/geometry-dash`.

## Project summary
An inspired-by remake with **original assets** (no ripped sprites/music). Stack: **SDL3 + SDL_ttf3 + SDL_mixer3** via CMake FetchContent. Linux first, portable code. Unit tests with **doctest** on all logic; headless smoke check (dummy video driver, N frames, exit 0).

## Domain decisions (authoritative)
- **Gamemodes**: Cube (tap to jump; buffered input) and Ship (hold to fly). **Portals**: gravity flip and speed changes. **Hazards/blocks**: spikes, blocks, platforms. **Practice mode**: place checkpoints, respawn at last checkpoint, toggleable. **Attempts counter** per level. **Instant death-restart** in normal mode.
- **Levels are data, not code**: JSON files listing objects against a fixed **object palette** (every type the engine supports: spike, block, platform, portals, end-wall, decorations). The palette is documented in `docs/LEVEL-FORMAT.md` and both the loader and validator enforce it.
- **10 levels, 30–60 seconds each, difficulty ramping** — designed by a **Claude sub-agent** working directly in the JSON format, validated by the validator script after every level (only palette objects; no unreachable gaps: no required jump taller than cube max, no impossible ship corridors).
- **Music**: 10 CC0 tracks (one per level), sourced by an asset-search sub-agent; gameplay synced loosely (level length ~ song length); credited in `LICENSES.md` with source URLs.
- Physics must be **deterministic and frame-rate independent** (fixed timestep). Collision is AABB/grid-based against level objects.

## Asset policy
CC0 preferred, CC-BY with attribution, no NC-only licenses; everything logged in `LICENSES.md`. Delegate asset-search sub-agents for: player cube/ship sprites, tile/block/spike tileset, background, portals, particle/ground art, and the 10 tracks. Programmatic original art fallback acceptable — note in handoff.md.

---

## Phase 0 — Divide & assign models
Model tiers: **easy → fastest model; hard → strongest reasoning model**. Sections:

1. **Scaffold + engine core** *(medium)* — CMake + FetchContent, fixed-timestep loop, input, state machine skeleton, AABB utilities.
2. **Gamemodes & portals** *(hard — strongest model)* — cube jump physics with exact tuned constants, ship flight, gravity/speed portals, collision (death on hazard, land on blocks/platforms), practice checkpoints. Heavy doctest coverage on the physics/collision math with synthetic levels.
3. **Level format, loader & validator** *(medium)* — JSON schema per `docs/LEVEL-FORMAT.md`, loader, validator script (palette conformance + completability invariants), doctest tests on both.
4. **Renderer + assets + audio** *(medium)* — camera follow, parallax background, object rendering from tileset, music per level + SFX, death/win effects.
5. **Menus & meta** *(easy-medium)* — level select (10 entries, attempts + best-% shown), practice-mode toggle, pause, attempts counter persistence.
6. **The 10 levels** *(hard, content — Claude sub-agent, separate from engine)* — author 10 levels in JSON with ramping difficulty, run the validator on each, iterate until valid; assign a CC0 track to each.

## Phase 1 — Delegate to sub-agents
For EACH section:
- Spawn one **implementer sub-agent** with its spec, shared conventions, and test obligations.
- Spawn one **reviewer sub-agent** that verifies against the section's **written test script**: doctest suites + `TESTS.md` checklist with exact commands (build, `ctest`, validator run per level, smoke test) + interactive playthrough checks the reviewer performs (level 1 beatable, portals function, practice checkpoints work).
- Reviewer loops with implementer until green; real command output required. The level section's reviewer additionally beats (or watches a scripted run of) levels 1–3 and sanity-checks 4–10 in a debug fly mode.

## Phase 2 — Integration & handoff
- Integrate: loader feeds the engine; single physics-constants header shared by tests and game; music assignment matches level lengths; validator runs in the build's test step for all 10 levels.
- Verify: clean build, `ctest` green, validator green on all 10 levels, smoke test green, TESTS.md ticked, LICENSES.md complete.
- Write **`handoff.md`**: what exists, how to build/run/test, level-format authoring guide (so new levels can be added), architecture, known gaps. State + resume only — no v2 ideas.