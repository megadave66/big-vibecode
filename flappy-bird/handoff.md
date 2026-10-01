# Handoff — Flappy Remake (SDL3 + C++20)

A playable, inspired-by Flappy Bird remake: get-ready screen, tap to flap, endless pipes
with fair random gaps, score, game-over panel with restart, and a saved best score.
Sounds play on flap, score and hit. Art is a CC0 Kenney plane plus original code-drawn pipes and ground.

## Build, run, test

Needs CMake ≥ 3.25, Ninja and a C++20 compiler. All libraries download at configure time
as pinned release tarballs (no git, no system libraries).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # first run downloads deps
cmake --build build
ctest --test-dir build --output-on-failure              # 58 tests
./build/flappy                                          # play
```

Controls: Space, Up, W, left click or touch to flap. Esc quits.

Command-line options (for tests and debugging):

| Option | Meaning |
|---|---|
| `--smoke N` | Run N fixed ticks with no real-time wait, scripted input, then print `SMOKE OK ...` and exit 0 |
| `--screenshot PATH --frames N` | Run N scripted ticks, save the last frame as a PNG, exit |
| `--script auto\|idle\|die` | Scripted input for headless modes: autopilot / never press / play to score 3 then crash |
| `--seed S` | Pipe seed (default: time based) |
| `--best-file PATH` | Best-score file (default: `SDL_GetPrefPath("big-vibecode","flappy-remake")/best.txt`, on macOS `~/Library/Application Support/big-vibecode/flappy-remake/best.txt`) |

Headless runs need `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`. ctest sets these itself.

## What exists

| Path | What it does |
|---|---|
| `CMakeLists.txt` | FetchContent for SDL3 3.4.16, SDL3_ttf 3.2.2, SDL3_mixer 3.2.4, FreeType 2.13.3, doctest 2.5.3 (URL + SHA256). Targets `flappy_core`, `flappy`, `flappy_tests`. Strict warnings + `-Werror` on our targets (`FLAPPY_WERROR`, default ON). Copies `assets/` next to the binary. Registers ctest cases. |
| `cmake/FindFreetype.cmake` | Shim so SDL_ttf's `find_package(Freetype)` uses the fetched FreeType. |
| `cmake/smoke_gameover.cmake` | ctest script: plays, dies, reaches GameOver, checks the best-score file. |
| `src/core/config.hpp` | Every gameplay constant (world size, tick rate, gravity, flap, pipe speed, gap range, margins). |
| `src/core/collision.*` | `Rect` and strict AABB `intersects`. |
| `src/core/bird.*` | Bird physics: gravity, flap, terminal speed, rotation, hitbox, ground/ceiling checks. |
| `src/core/pipes.*` | `GapGenerator` (bounded, seeded, limited change between gaps) and `PipeField` (scroll, spawn, despawn, score, collide). |
| `src/core/game.*` | State machine: GetReady → Playing → Dying → GameOver → GetReady. Raises one-shot events for sound and saving. |
| `src/core/best_score.*` | Load/save best score as text; atomic write via temp file + rename; never throws. |
| `src/core/autopilot.*` | Scripted player for smoke/screenshot runs and the "gaps are passable" test. |
| `src/app/main.cpp` | SDL init, window, fixed-timestep loop, input, CLI, headless modes, event → audio/save wiring. |
| `src/app/render.*` | Draws background, pipes, ground, rotated animated plane, HUD, get-ready and game-over screens (SDL_ttf). Falls back to flat colours / debug text if an asset is missing. |
| `src/app/audio.*` | SDL3_mixer sound effects; tries `.ogg` then `.wav`; silent no-op if audio or files are missing. |
| `tests/*.cpp` | 55 doctest cases: config, bird, collision, pipes, autopilot, game, best score. |
| `tools/gen_art.py` | Generates the original `pipe_body.png`, `pipe_cap.png`, `ground.png` (needs Pillow). |
| `assets/` | Sprites, sounds, font. Sources and licences in `LICENSES.md`. |
| `TESTS.md` | Test checklist with exact commands and real results, plus the manual playthrough list. |

## Architecture notes

- **Two layers.** `src/core` is pure C++ with no SDL; all of it is unit tested. `src/app` is the only SDL code.
- **Fixed timestep.** The sim runs at 120 Hz (`kDt`). The loop accumulates real time, clamps a frame to 0.25 s,
  and calls `Game::tick(kDt)` as often as needed. Physics never sees a variable dt, so play is frame-rate independent.
- **One input verb.** Every tap calls `Game::press()`; its meaning depends on state. After game over, a press only
  counts after `kGameOverInputDelay` (0.6 s) to stop accidental restarts.
- **Events, not callbacks.** `Game` sets flags (`flapped`, `scored`, `hit`, `game_over`); `main.cpp` drains them
  after each tick to play sounds and save a new best.
- **Fair gaps.** Gap height is random in [100, 125] px, at least 50 px from ceiling and ground, and each gap centre
  moves at most 140 px from the last. Each restart derives a new seed, so runs differ but stay reproducible.
  The autopilot test flies 20 seeds for 60 s each without a hit (lowest score 43).
- **Hitbox.** The bird hitbox is the 34x24 box shrunk 4 px per side and ignores rotation, which is a little forgiving.
  The pipe caps overhang the collision box by 3 px per side (visual only).
- **Rendering cost.** The background is baked once into a 288x512 texture, and text objects are cached.
  Headless runs use a 1x window (288x512) to keep the software renderer fast. Interactive runs use 2x.

## Known gaps

- **Not hand-played yet.** No human has played it in a real window. The playthrough list in `TESTS.md` is
  marked "MANUAL — pending user". All automated checks, and screenshots of all three screens, pass.
- **Sound not heard.** Sounds load and decode under the dummy driver, but no one has listened to them. The flap
  sound is a short UI click (Kenney Interface Sounds), not a wing sound.
- **macOS only so far.** Built and tested only on macOS (Apple Silicon). The code and CMake avoid platform-specific
  calls, and MSVC warning flags are in place, but Windows and Linux builds have not been tried.
- **Screenshots are 288x512.** Headless mode renders at 1x, so screenshots are smaller than the real window.
- **Best-score path.** If `SDL_GetPrefPath` fails, the game logs a warning and runs without saving the best score.
- **Not original Flappy Bird art.** The "bird" is a Kenney biplane. Pipes and ground were drawn with code
  (`tools/gen_art.py`) because the plane pack's rock spikes did not work as pipes.
- **Random numbers vary by standard library.** `std::uniform_real_distribution` output can differ between
  standard libraries, so a given seed may give different gaps on another platform. Tests check bounds, not exact values.

## Resume instructions

1. Run the build and test commands above. Expect `100% tests passed out of 58`.
2. Work through the manual playthrough list in `TESTS.md` with `./build/flappy`.
3. Tune feel only in `src/core/config.hpp`, then re-run `ctest`. The autopilot test fails if a change makes gaps unpassable.
4. To regenerate the code-drawn art: `python3 tools/gen_art.py` (keep the ground pattern at `kGroundPatternWidth` = 24 px).
