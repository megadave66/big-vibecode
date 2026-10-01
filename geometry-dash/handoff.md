# Handoff — Geometry Dash Remake (SDL3 + C++20)

State on 2026-10-01. Everything below was checked with real command output (see `TESTS.md`).

## What exists
- A playable Geometry Dash–inspired game with 10 levels, original procedural art, and CC0 music.
- Gamemodes: cube (tap to jump, buffered press, hold re-jumps) and ship (hold to fly).
- Portals: gravity (down/up), mode (cube/ship), speed (slow/normal/fast/faster).
- Objects: spikes (up/down), blocks, platforms, end wall, decorations.
- Normal mode with instant death-restart. Practice mode with checkpoints.
- Attempts counter and best % (normal and practice) per level, saved to disk.
- Menus: main menu, level select (10 cards), pause, level complete.
- Tools: `gd_validate` (static level checks), `gd_solve` (finds a winning input), `gd_replay`
  (re-runs an input file headless and proves the level is beatable).

## Build, run, test
From this folder (macOS, cmake ≥ 3.24, Ninja, a C++20 compiler; first configure downloads deps):
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure     # 24 tests
build/geometry_dash                            # play
```
Controls: Space / Up / left click = jump or fly. Esc = pause / back. Enter = confirm. Arrows =
menu. P = practice toggle. In practice: Z or C = place checkpoint, X = remove last. R = restart.

Useful flags (full list in `docs/ARCHITECTURE.md`): `--level N`, `--practice`, `--fly` (cannot die),
`--replay FILE`, `--hitboxes`, `--mute`, `--save PATH`, `--smoke N`,
`--screenshot OUT.bmp --frame K`, `--frame-every K --screenshot-dir DIR [--max-shots N]`.
Headless runs need `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`.
Convert a shot with `sips -s format png X.bmp --out X.png`.

Save file: SDL pref path `big-vibecode/GeometryDashRemake/progress.json`.

## Last verified results (clean build)
- ctest 24/24: `unit`, `smoke`, `smoke_game`, `smoke_menus`, `validate_level01..10`, `replay_level01..10`.
- `gd_tests`: 131 test cases, 34,018 assertions, all pass.
- `gd_validate`: OK for all 10 levels. `gd_replay`: WIN for all 10 replays.
- Compiler warnings: 0 in our code; 1 in vendored FreeType.

| # | name | length | track | features | solver min slack |
|---|---|---|---|---|---|
| 1 | First Steps | 51.9 s | track01 | cube, single spikes, steps | 10 ticks |
| 2 | Double Up | 41.0 s | track02 | double spikes, platforms | 10 |
| 3 | Flip Side | 51.9 s | track03 | triple spike, gravity flips | 10 |
| 4 | Take Flight | 52.2 s | track04 | first ship section | 7 |
| 5 | Overdrive | 53.9 s | track05 | fast speed, cube/ship mix | 6 |
| 6 | Upside Tempo | 53.4 s | track06 | cube gravity flips, slow/fast | 8 |
| 7 | Narrow Skies | 53.4 s | track07 | ship slopes, gap 4–5 | 6 |
| 8 | Pulse Runner | 50.5 s | track08 | faster speed, dense rhythm | 3 |
| 9 | Gravity Well | 53.4 s | track09 | ship gravity flips, 18 portals | 3 |
| 10 | Final Overload | 50.1 s | track10 | everything | 3 |

1 tick = 1/240 s. Levels 1–3 were beaten by scripted input in the real game. Levels 4–10 were
checked in fly mode and by their replays.

## Architecture
- `src/core/` — pure C++ (no SDL), library `gd_core`. Tested by doctest.
  - `PhysicsConstants.h`: the single source of physics numbers. Sim, validator, solver, game and
    tests all read it.
  - `Level`, `LevelLoader` (JSON via nlohmann/json), `Validator`.
  - `Sim`: deterministic fixed step (240 Hz, `double`). One `step(held)` per tick. Cheap
    `snapshot()`/`restore()`. Events for audio and effects. `setInvincible` for fly mode.
  - `Practice` (checkpoint stack), `Replay` (text format + runner), `Solver` (DFS over inputs).
  - `Progress` (save file), `Camera`, `Interp`, `Particles`, `FixedTimestep`, `StateMachine`, `Aabb`.
- `src/app/` — SDL3 front end. `App` (loop, flags, states), `GameScreen` (play, death, practice),
  `GameRenderer` (all procedural art), `Menus`, `Audio` (SDL_mixer 3), `Text` (SDL_ttf), `Gfx`.
- The app runs the sim through `FixedTimestep` and interpolates for drawing. The sim never reads
  the clock, so results do not depend on frame rate (tested at 30/60/144/240 fps).
- `tools/`: `gd_validate`, `gd_solve`, `gd_replay`. `tools/gen_music.py` is an unused fallback
  chiptune generator.
- Dependencies (pinned tarballs with SHA256, fetched by CMake): SDL3 3.4.16, SDL3_ttf 3.2.2
  (+ SDL FreeType fork at a pinned commit), SDL3_mixer 3.2.4 (WAV + stb_vorbis only),
  nlohmann/json 3.12.0, doctest 2.5.3.

## Adding a level
1. Read `docs/LEVEL-FORMAT.md`. It lists every object type and field, and the validator rules.
2. Copy a level to `levels/levelNN.json`. Set `id` to NN, `music` to a file under `assets/`.
3. Place objects. Keep the first 8 blocks clear. Leave at least 3 blocks after a portal.
   The cube jumps about 2.2 blocks high and 4.8 blocks long at normal speed.
4. Run, in order:
   ```
   build/gd_validate levels/levelNN.json --data .
   build/gd_solve levels/levelNN.json -o replays/levelNN.replay --data .
   build/gd_replay levels/levelNN.json replays/levelNN.replay --data .
   ```
   If the solver fails, it prints the furthest x it reached. Fix the level near that x.
   Check the solver's `min_slack`: aim for 6+ ticks on easy levels, never below 3.
5. Re-run `cmake -S . -B build` so ctest picks up the new files.
6. Level select shows ids 1–10. A level 11+ needs `kLevelCount` raised in `src/app/Menus.h` and a card layout change in `src/app/Menus.cpp`.

## Assets and licences
All listed in `LICENSES.md`.
- Music: 10 real CC0 tracks by SubspaceAudio (Juhani Junkala), OpenGameArt. Trimmed to about
  41–52 s, faded, re-encoded to OGG. No generated tracks are used.
- SFX: Kenney (CC0). Font: Russo One (OFL-1.1).
- Art: all drawn in code (no sprite files). This is the "procedural original art" fallback.

## Known gaps
- Manual checks are still pending the user (6 items in `TESTS.md`): real-window play, feel of
  jump and ship, hearing music and SFX, mouse click-through, playing each level by hand.
- Tightest single-press windows are 7–8 ticks (about 30 ms): level 8 near x≈337, level 9 near
  x≈93, level 10 near x≈346. Hard but not frame-perfect. Loosen these if playtests find them unfair.
- Track tempo (bpm) is unknown, so obstacle timing is only loosely matched to the music.
- Tracks are cut from longer loops; the cut is hidden by a 2 s fade-out.
- Portals are full-height column triggers. A player cannot miss one by flying over it.
- Only macOS was built and tested. The code uses no Apple-only APIs, but Linux and Windows
  builds are untested.
- One compiler warning remains in vendored FreeType (not our code).
- `levels/_sample.json` is a test level for the tools. ctest ignores it.
