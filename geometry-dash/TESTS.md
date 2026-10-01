# Test script

Each section appends its own part below. Tick a box only after running the command and seeing real output.

## Section 1 — Scaffold

- [x] Configure: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` (fetches deps; first run takes about a minute)
- [x] Build: `cmake --build build` (no warnings in our code)
- [x] All tests: `ctest --test-dir build --output-on-failure` (at section 1: 2/2; now 24/24, see Integration)
- [x] Unit tests alone: `build/gd_tests` (at section 1: 28 cases / 2124 assertions; now 131 / 34018, see Integration)
- [x] Smoke: `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy build/geometry_dash --smoke 300 --mute` (exit 0)
- [x] Screenshot: `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy build/geometry_dash --screenshot /tmp/gd_shot.bmp --frame 5 --mute`
- [x] Convert: `sips -s format png /tmp/gd_shot.bmp --out /tmp/gd_shot.png` (menu placeholder visible: banner, two buttons, ground)
- [ ] MANUAL — pending user: `build/geometry_dash` opens a 1280x720 window; Enter walks menu -> level select -> play; Esc pauses; closing the window exits.

## Section 3 — Level format & validator

- [x] Configure + build: `cd ROOT && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` (no warnings in our code)
- [x] All tests: `ctest --test-dir build --output-on-failure` (unit + smoke pass; plus validate_levelNN / replay_levelNN once those files exist)
- [x] Loader tests: `build/gd_tests -tc="loader:*"` (14 test cases, 133 assertions)
- [x] Validator tests: `build/gd_tests -tc="validator:*"` (13 test cases, 60 assertions)
- [x] Level basics: `build/gd_tests -tc="duration*,portal*,speedValue*,toString*"` (9 test cases, 24 assertions after the duration fix)
- [x] Sample level: `build/gd_validate levels/_sample.json --data .` prints `OK _sample.json (37.4 s, 68 objects)`, exit 0
- [x] Failure output: a copy of the sample with a spike at x=5 and a missing music file prints `[start-clear]` and `[music]` lines, exit 1; a file missing keys prints `[load] ...` lines, exit 1
- [x] After the 10 real levels exist: `ctest --test-dir build -R validate_ --output-on-failure` (10/10 pass, Integration run)

## Section 2 — Physics

- [x] Configure + build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` (no warnings in Sim/Replay/Practice/gd_replay)
- [x] All tests: `ctest --test-dir build --output-on-failure` (unit + smoke passed)
- [x] Physics unit tests: `build/gd_tests -sf="*test_sim*,*test_replay*,*test_practice*"` (48 test cases, 8902 assertions, all pass)
  - `test_sim.cpp` (20): flat-run end tick at every speed, apex/air time vs `kCubeJumpApex`/`kCubeAirTime`, jump length vs `cubeJumpLength`, hold re-jump, jump buffer, land on block, side hit, head hit, spike death + near miss, dir=down spike, no tunnelling (max fall onto a 0.5 platform at faster; thin wall), rotation snap, events, determinism, frame-rate independence (30/60/144/240 fps), snapshot/restore + Sim copy, fly mode, 40k-object level
  - `test_sim_modes.cpp` (13): gravity portal (ceiling, block bottoms, side death), speed portal at the exact trigger tick, portals fire at any height, mode portals cube→ship→cube with clamped vy, ship clamp + tilt, ship slides on ceiling/ground and block tops/bottoms, ship wall death, ship and cube under gravity up
  - `test_sim_limits.cpp` (5): triple spike clearable at normal and 4 not; `cubeMaxSpikeRun(speed)` clearable and +1 not at every speed; `kCubeMaxClimb` step climbable; a `kShipMinGap` zig-zag corridor at `kShipMaxSlope` flown by a simple controller at every speed
  - `test_replay.cpp` (6), `test_practice.cpp` (4): parse errors with line numbers, round trip, runReplay win/timeout/death; checkpoint add/remove/respawn
- [x] `gd_replay` exit codes on a scratch level (no committed levels yet): win → `WIN ...` exit 0; `end` too early → `TIMEOUT ...` exit 1; no input → `DEAD ...` exit 1; bad replay line → errors with line numbers, exit 2; level id mismatch → exit 2
- [x] Per-level replays: `ctest --test-dir build -R replay_` (10/10 pass, Integration run)
- [ ] MANUAL — pending user: in game, cube jump feels like GD (about 2 blocks high; a triple spike is just clearable at normal speed); ship feels controllable

## Solver

- [x] Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` (run in a separate `build-solver` dir; no warnings in Solver.cpp / gd_solve.cpp / test_solver.cpp)
- [x] Solver unit tests: `build/gd_tests -tc="*olver*"` (5 test cases, 30 assertions: flat level 0 presses, spike row needs presses and replays to a win, ship corridor, impossible 4-high wall → no solution, determinism)
- [x] Full unit suite still passes: `build/gd_tests` (124 test cases, all pass)
- [x] Synthetic 58 s level (cube spikes/blocks, ship pillars, speed + gravity portals): `build/gd_solve synth50.json -o synth50.replay` prints `SOLVED ... presses=40 win_tick=14038 ... min_slack=8 seconds=0.03`, exit 0; `build/gd_replay synth50.json synth50.replay` prints `WIN`, exit 0
- [x] Sample level: `build/gd_solve levels/_sample.json -o <tmp>.replay` → `SOLVED presses=27 min_slack=2`, exit 0
- [x] No solution: same level with a 4-high wall 20 blocks before the end → `NO SOLUTION furthest_x=647.361 tick=13584 ...`, exit 1, 2.3 s; with `--max-seconds 0.5` → `(time limit hit)`, exit 1
- [x] Bad input: missing `-o` → usage, exit 2; missing level file → load error, exit 2
- [x] Determinism: two runs on the same level write byte-identical replays (`cmp`)
- [x] Per-level replays: for each NN, `build/gd_solve levels/levelNN.json -o replays/levelNN.replay`, then `ctest --test-dir build -R replay_` (done by the levels section; 10/10 pass in Integration)

## Section 4 — Renderer & audio

Headless env for all game runs: `export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`. Shots go to `/tmp/gd-shots/`. Convert with `sips -s format png X.bmp --out X.png`.

- [x] Configure + build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` (no warnings in our code)
- [x] All tests: `ctest --test-dir build --output-on-failure` (10/10: unit, smoke, smoke_game, smoke_menus, validate_level01-03, replay_level01-03)
- [x] Unit tests: `build/gd_tests` (131 test cases, 34018 assertions); new ones: `build/gd_tests -sf="*test_camera*,*test_particles*"` (15 test cases: camera lead/clamp/dead zone/smoothing/ship framing, angle lerp, xorshift, particles)
- [x] Smoke, menu walk: `build/geometry_dash --smoke 300 --mute` prints `smoke ok ... state=Playing` (exit 0)
- [x] Smoke, whole level in fly mode: `build/geometry_dash --level 1 --fly --smoke 3300` prints `state=Complete percent=100 won=1`, `audio=on` with the dummy audio driver (exit 0)
- [x] Replay drives the game: `build/geometry_dash --level 1 --replay replays/level01.replay --smoke 3300` reaches `state=Complete`, attempts=1
- [x] Bad level: `build/geometry_dash --level-file /nonexistent.json --smoke 5` exits non-zero (see below)
- [x] Screenshots of `levels/_sample.json` (`--level-file levels/_sample.json --mute --screenshot X.bmp --frame K`), all inspected:
  - `--frame 20` cube on ground, "Attempt 1", trail dust, spike, progress bar
  - `--frame 50 --hold 40-60 --hitboxes` mid-jump, cube rotated, outer/inner/spike hitboxes line up with the art
  - `--frame 82` death burst (squares + ring)
  - `--fly --frame 568` speed portal with chevrons; `--fly --frame 800` ship with flame, pink mode portal, corridor framed
  - `--fly --frame 1450` gravity-up cube on the ceiling with hanging spikes
  - `--practice --checkpoint-at 60,100 --hold 80-95 --frame 130` green diamond checkpoint marker, PRACTICE tag
- [x] Level 1 with its replay (`--level 1 --replay replays/level01.replay --frame K`): frame 1280 mid-jump over a spike, 3040 end wall approaching, 3130 win burst, 3200 "Level Complete" overlay
- [x] Frame sampling: `--level 1 --replay replays/level01.replay --frame-every 160 --screenshot-dir DIR --mute` writes 20 BMPs and stops
- [ ] MANUAL — pending user: hear music and SFX in a real window (music per level and restart on death, death / checkpoint / portal / complete sounds, `--mute` silences all)
- [ ] MANUAL — pending user: real window feel (Space/Up/click jump, Esc pause, Z/C place, X remove, R restart, P practice toggle; resize keeps 16:9 letterbox; text sharp on a Retina display)

New flags: `--level-file <path>`, `--hitboxes` (also outlines blocks and platforms in yellow), `--max-shots N` (cap for `--frame-every`, default 20, max 200; warns on stderr at the cap; stops at level complete), `--frame-every K`, `--screenshot-dir DIR`, `--hold A-B[,C-D]` (scripted jump hold, frame numbers), `--checkpoint-at F[,F]`.

## Section 5 — Menus & meta

Headless env: `export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`. Shots and the fake-progress fixture (`fix.json`) live in `/tmp/gd-menus/`.

- [x] Configure + build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` (no warnings in our code)
- [x] All tests: `ctest --test-dir build --output-on-failure` (10/10: unit, smoke, smoke_game, smoke_menus, validate_level01-03, replay_level01-03)
- [x] Unit tests: `build/gd_tests` (131 test cases, 34018 assertions); progress: `build/gd_tests -tc="progress:*"` (7 cases: round trip, missing file, corrupt files, clamping, best % only increases, attempts increment, ignored ids)
- [x] Menu smoke: `build/geometry_dash --smoke 200 --mute --save X.json --menu-script "enter,right,p,enter,wait,wait,wait,esc,down,enter,wait,esc,down,down,down,enter,wait,click:640:367"` prints `state=LevelSelect`; X.json holds level 2 with 2 attempts. Run by ctest `smoke_menus` (also a corrupt-save run that must exit 0).
- [x] Screenshots (all inspected, `--save /tmp/gd-menus/fix.json`): main menu `--frame 20`; level select `--frame 40 --menu-script "enter,down,right,p"` (progress, ticks, "missing" cards, practice toggle); pause `--level 1 --frame 60 --menu-script "wait,wait,esc,down"`; complete `--level 1 --replay replays/level01.replay --frame 3250` (attempts, jumps, time, best, buttons)
- [x] Font: Russo One (OFL-1.1) replaces Kenney Future; K/X/H/E/M clear in the screenshots
- [ ] MANUAL — pending user: real-window click-through (mouse hover and click on every menu, Esc/Enter/arrows/P, menu SFX heard, progress file at the SDL pref path survives a restart, window close saves)

New flags: `--save <path>` (progress file; default is the SDL pref path `big-vibecode/GeometryDashRemake/progress.json`; headless `--smoke/--screenshot` runs without `--save` keep progress in memory only), `--menu-script "k1,k2,..."` (one key per 6 frames: up down left right enter esc p r space wait mouse:X:Y click:X:Y).

## Section 6 — Levels

Built in a separate dir: `cmake -S . -B build-levels -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-levels --target gd_validate gd_replay gd_solve` (dir deleted afterwards).
Per level NN = 01..10:
- `build-levels/gd_validate levels/levelNN.json --data .`
- `build-levels/gd_solve levels/levelNN.json -o replays/levelNN.replay --data .`
- `build-levels/gd_replay levels/levelNN.json replays/levelNN.replay --data .`

Slack targets: levels 1–3 ≥ 10 ticks, 4–7 ≥ 6, 8–10 ≥ 3. Track length from `assets/music/tracks.json`; level duration must be within ±3 s.

| # | name | track (s) | duration | objects | validator | solver presses | min slack | replay | features |
|---|---|---|---|---|---|---|---|---|---|
| 1 | First Steps | 51.96 | 51.9 s | 42 | OK, exit 0 | 37 | 10 | WIN tick 12456, exit 0 | cube, single spikes, 1-block steps |
| 2 | Double Up | 41.14 | 41.0 s | 52 | OK, exit 0 | 24 | 10 | WIN tick 9833, exit 0 | double spikes, platforms over spike beds, small stairs |
| 3 | Flip Side | 52.00 | 51.9 s | 66 | OK, exit 0 | 32 | 10 | WIN tick 12467, exit 0 | first triple (from a step), 2 short gravity flips onto ceiling 9 |
| 4 | Take Flight | 52.00 | 52.2 s | 106 | OK, exit 0 | 36 | 7 | WIN tick 12525, exit 0 | first ship section, wide corridor with pillars |
| 5 | Overdrive | 52.00 | 53.9 s | 121 | OK, exit 0 | 40 | 6 | WIN tick 12942, exit 0 | fast portal, quads, two ship sections at fast |
| 6 | Upside Tempo | 52.00 | 53.4 s | 115 | OK, exit 0 | 47 | 8 | WIN tick 12819, exit 0 | 5 cube gravity flips, slow and fast sections, hanging spikes, ceiling stairs |
| 7 | Narrow Skies | 52.00 | 53.4 s | 213 | OK, exit 0 | 35 | 6 | WIN tick 12814, exit 0 | two ship sections, stepped slopes, gap 4–5 |
| 8 | Pulse Runner | 52.00 | 50.5 s | 215 | OK, exit 0 | 60 | 3 | WIN tick 12118, exit 0 | faster speed, one jump per beat, 5-runs, ship at faster |
| 9 | Gravity Well | 52.00 | 53.4 s | 148 | OK, exit 0 | 37 | 3 | WIN tick 12817, exit 0 | 3 ship sections with gravity flips in flight, upside-down cube, 18 portals |
| 10 | Final Overload | 52.00 | 50.1 s | 225 | OK, exit 0 | 51 | 3 | WIN tick 12018, exit 0 | everything: fast/faster/slow, gravity in cube and ship, gap-4 slopes |

- [x] All 10 levels: validator OK, solver SOLVED, replay WIN (all exit 0), all slack targets met
- [x] Durations within ±3 s of each track and inside 30–60 s
- [x] First 8 blocks clear (validator start-clear rule); no spike, block or platform starts within 3 blocks after any portal (checked by script over all level files)
- [x] `ctest --test-dir build -R "validate_|replay_" --output-on-failure` in the main build (20/20 pass, Integration run)
- [ ] MANUAL — pending user: play each level; check it reads well, jumps feel on the beat, nothing appears without warning
- Note: the solver centres each hold by at most 8 ticks (`slackCap` 16), so cube levels report about 8 unless a hold's latest press falls late in the 6-tick search grid. Levels 1–3 reach 10 by moving some obstacle groups by 0.5–3 blocks. The real timing windows are wider (a single spike at normal speed gives about 60 ticks).

## Section 6 review (level reviewer)

- [x] Levels 1–3 beaten by scripted input in the real game: `build/geometry_dash --level N --replay replays/levelNN.replay --mute --save <tmp> --frame-every K --screenshot-dir <dir>` ends in Level Complete; the save shows `completed: true`; sampled frames show the cube clearing obstacles.
- [x] Replays are real: removing any single press from a copy makes `gd_replay` fail, for every level.
- [x] Levels 4–10 in debug fly mode (`--level N --fly`, frame sampling): portals visible with reaction room, corridors readable. Each replay also reaches Level Complete in the real game.
- [x] Fixed after review: three gravity portals drawn inside blocks (level 10 x=577 y→5.5, x=597 y→1; level 9 x=126 y→4.5). Portal `y` is visual only; validate + replay re-run: OK / WIN.

## Integration (lead, clean build)

Run from ROOT after `rm -rf build`:

- [x] `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` → exit 0, 0 CMake warnings (all tarballs SHA256-checked)
- [x] `cmake --build build` → exit 0, 367 steps; 1 compiler warning, in vendored FreeType (`zutil.h:172 OS_CODE redefined`); 0 in our code
- [x] `ctest --test-dir build --output-on-failure` → `100% tests passed out of 24` (unit, smoke, smoke_game, smoke_menus, validate_level01–10, replay_level01–10)
- [x] `build/gd_tests` → 131 test cases, 34018 assertions, 0 failed
- [x] `build/gd_validate levels/level*.json --data .` → OK ×10, exit 0
- [x] `build/gd_replay levels/levelNN.json replays/levelNN.replay --data .` → WIN ×10
- [x] `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy build/geometry_dash --smoke 300 --mute` → `smoke ok ... state=Playing`, exit 0
- [x] Frame inspection: level 7 frame 1500 (ship in stepped corridor) and level 3 frame 1400 (flipped cube on ceiling, hanging spikes, gravity portal) render correctly
