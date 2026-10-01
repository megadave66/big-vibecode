# Test log

All commands run from the project root. Every ticked item was run and seen to pass.
Final integration run: 2026-10-01, macOS (Apple Silicon), Apple clang, CMake 4.4, Ninja.

## Final integration (Phase 2)

- [x] Clean configure, zero warnings: `rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- [x] Build, zero warnings (our code and deps): `cmake --build build` — 0 `warning:` lines in the log
- [x] Full suite: `ctest --test-dir build --output-on-failure` — `100% tests passed out of 58`
      (55 doctest cases + `smoke` + `screenshot` + `smoke_gameover`)
- [x] Unit tests direct: `./build/flappy_tests` — `test cases: 55 | 55 passed | 0 failed`, `assertions: 979128 | 979128 passed`
- [x] Smoke: `cd build && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./flappy --smoke 1200 --seed 1 --best-file /tmp/flappy_smoke_best.txt` — `SMOKE OK ticks=1200 score=6 best=0 state=1`, exit 0
- [x] Game over + best-score file: ctest `smoke_gameover` — `SMOKE OK ticks=2400 score=3 best=3 state=3`, `best score persisted: 3`
- [x] Screens checked by eye (PNG, 288x512):
      `cd build && export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`
      - `./flappy --screenshot ready.png --frames 60 --seed 1 --script idle --best-file b.txt` — GET READY title, hint, BEST, plane, ground
      - `./flappy --screenshot play.png --frames 900 --seed 1 --best-file b.txt` — pipes with caps, score 4, tilted plane
      - `./flappy --screenshot over.png --frames 2400 --seed 1 --script die --best-file b2.txt` — panel with GAME OVER, SCORE 3, BEST 3, NEW BEST!, PRESS SPACE

## Section 1 — Scaffold

- [x] Configure, build, zero warnings from our targets (Reviewer: PASS)
- [x] `ctest` registers doctest cases (doctest_discover_tests), `smoke` and `screenshot`
- [x] Smoke and screenshot exit 0 with dummy video and audio drivers

Reviewer (Section 1): PASS. URL tarballs pinned by SHA256, all deps static, no SDL in src/core, fixed-timestep loop with clamp.

## Section 2 — Bird

- [x] `./build/flappy_tests --test-case="bird:*"` — 13 cases pass

Reviewer (Section 2): PASS. Trapezoidal integration (exact under constant gravity) is an accepted deviation from semi-implicit Euler. Frame-rate independence checked at 1/60, 1/120 and 1/240 s.

## Section 3 — Pipes and collision

- [x] `./build/flappy_tests --test-case="collision:*,pipes:*,autopilot:*"` — 15 cases pass
- [x] Gap bounds hold for 50 seeds x 2000 gaps; generator deterministic; heights vary
- [x] Spawn spacing, off-screen removal, scroll speed, score once per pipe, reset replays seed
- [x] Autopilot survives 60 s on 20 seeds with no hit, score >= 30 (min 43) — gaps are passable

Reviewer (Section 3): PASS. Minor findings (reset test, named autopilot constants, header comment) fixed and re-verified.

## Section 4 — Game flow and rendering

- [x] `./build/flappy_tests --test-case="game:*,best:*"` — 25 cases pass
- [x] Three screens checked by eye (see Final integration)

Reviewer (Section 4): PASS after fixes. Fixed: vacuous "Dying ignores press" test, added "frozen in Dying" test,
background baked once (was 55 ms per frame in software), TTF text cached, flat fallback for a missing cap,
no persistence when the pref path is unavailable.

## Section 5 — Assets and audio

- [x] All three sounds decode with the dummy audio driver (no `audio: could not load` lines in the smoke run)
- [x] Missing `hit.ogg` and `ui.ttf`: game still runs, exit 0, one log line
- [x] Every file in `assets/` has a row in LICENSES.md; licences checked on the source pages (CC0, OFL 1.1)

Reviewer (Section 5): PASS.

## Playthrough checklist (MANUAL — pending user)

These need a person at a real window. Run: `./build/flappy`

- [ ] Window opens at 576x1024 and scales when resized
- [ ] Resizing letterboxes (no stretch)
- [ ] Space, click and up arrow all flap
- [ ] Tap feel: flap response is snappy, not floaty
- [ ] Pipes feel fair: every gap is passable
- [ ] Score goes up by exactly 1 per pipe
- [ ] Hit plays a sound, the plane falls, then the game-over panel shows
- [ ] Press is ignored for about 0.6 s on game over, then restarts to get-ready
- [ ] Best score persists after quitting and relaunching
- [ ] ESC quits
- [ ] Flap, score and hit sounds are audible at a comfortable volume
