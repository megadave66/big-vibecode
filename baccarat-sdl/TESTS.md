# TESTS — Baccarat (SDL3 + C++)

## 1. Scaffold

Build system (FetchContent release archives with SHA256: SDL3 3.4.16, SDL_ttf 3.2.2 with vendored FreeType, SDL_mixer 3.2.4 WAV+OGG, doctest 2.5.3), app loop, renderer, scene skeleton, CLI flags.
Run on 2026-10-01, macOS arm64, Apple clang, Debug.

- [x] **Clean configure**: `rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `Configuring done`, SDL3_ttf backend `freetype`, SDL3_mixer backends `vorbis_stb wave`
- [x] **Build**: `cmake --build build` → no errors, no warnings from `src/`
- [x] **ctest**: `ctest --test-dir build --output-on-failure` → `100% tests passed out of 4` (smoke, smoke_screenshot, rules_tests, app_options_tests)
- [x] **Options tests**: `build/src/app/app_options_tests` → `test cases: 9 | 9 passed`, `assertions: 18 | 18 passed`
- [x] **Smoke**: ctest `smoke` = `baccarat --frames 120` with `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy` → exit 0
- [x] **Screenshot**: `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy build/src/app/baccarat --frames 5 --screenshot build/s.bmp` → 1280x720 BMP; inspected: green felt with "Baccarat" title in Lato Bold
- [x] **Logic-only build**: `cmake -S . -B build-logic -G Ninja -DBACCARAT_BUILD_GAME=OFF` → configures without fetching SDL

## 2. Rules engine

Pure C++20 library `baccarat_rules` (`src/rules/`), zero SDL. Suite: `rules_tests` (doctest).
Ticked items were run on 2026-10-01 (macOS arm64, Apple clang, Debug).

- [x] **Configure (no SDL)**: `cmake -S baccarat-sdl -B baccarat-sdl/build-logic -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBACCARAT_BUILD_GAME=OFF` → configures without error
- [x] **Build**: `cmake --build baccarat-sdl/build-logic` → builds `baccarat_rules` + `rules_tests`, 0 warnings under `-Wall -Wextra -Wpedantic`
- [x] **ctest**: `ctest --test-dir baccarat-sdl/build-logic -R rules_tests --output-on-failure` → `100% tests passed` (~3 s)
- [x] **doctest direct**: `baccarat-sdl/build-logic/src/rules/rules_tests` → `test cases: 39 | 39 passed`, `assertions: 23840 | 23840 passed`, `Status: SUCCESS!`
- [x] **Exhaustive banker table**: test `bankerShouldDraw: exhaustive 10x11 table` checks every banker total 0..9 x (player stood, third card 0..9) against a hand-written D/S grid → passes
- [x] **Exhaustive player draw**: test `playerShouldDraw: exhaustive 10x10 including naturals` → passes
- [x] **Brute force**: test `brute force: every 2-card start x every third card vs reference` plays 1,000,000 stacked rounds against an independent reference → `rounds == 1000000`, `mismatches == 0`
- [x] **Shoe**: 416 cards, each rank/suit 8 times; same seed → same order; golden sequence for seed 1 pinned (`6C 2C 6S 5S 9D JS 8D 10D`); cut card at 52 remaining; reshuffle; burn (flip 1, burn A=1/2-9/10-K=10) → all pass
- [x] **Settlement**: every bet x outcome, commission on $1/$5/$25/$100/$1000 (e.g. $5 Banker win returns 975), odd cents 1/19/20/21 (floor rule), multi-spot, tie push, zero bets → all pass
- [x] **Mutation check (manual, reverted)**: changing banker rule `3 draws unless P==8` to `P==9` in `src/rules/draw_rules.cpp` → 3 test cases fail (exhaustive table, branch cases, brute force). File restored; suite green again.
- [x] **Full build (with game)**: `ctest --test-dir baccarat-sdl/build --output-on-failure` → `rules_tests` passes (run by the reviewer, see below)

### Reviewer verification (2026-10-01, independent)

- [x] `cmake --build baccarat-sdl/build-logic && ctest --test-dir baccarat-sdl/build-logic --output-on-failure` → `100% tests passed out of 2`
- [x] `baccarat-sdl/build-logic/src/rules/rules_tests` → `test cases:    39 |    39 passed | 0 failed`, `assertions: 23840 | 23840 passed | 0 failed`, `Status: SUCCESS!`
- [x] `cmake --build baccarat-sdl/build && ctest --test-dir baccarat-sdl/build --output-on-failure` → `smoke`, `smoke_screenshot`, `rules_tests`, `app_options_tests` all Passed; `100% tests passed out of 4`
- [x] Code read against Punto Banco rules: `draw_rules.cpp` banker table (0-2 D; 3 unless P3=8; 4 on 2-7; 5 on 4-7; 6 on 6-7; 7 S; player stood → banker draws 0-5; naturals end hand), player draws 0-5. Test grid `kBankerTable` checked cell by cell by hand: correct. Deal order P,B,P,B,P3,B3 in `round.cpp`. Settlement: Banker 1:1 minus floor 5%, Tie 8:1, P/B push on tie, losers 0. Shoe: 8x52, own rejection-sampled uniform + Fisher-Yates (no std distributions), cut card 52, burn, reshuffle. No SDL includes, no mutable globals. `payoutLabel` built from `constants.hpp`.
- [x] Reviewer mutation (reverted): banker 6 rule `p == 6 || p == 7` → `p >= 5 && p <= 7` → `test cases: 39 | 36 passed | 3 failed`, `assertions: 23881 | 23866 passed | 15 failed`, `Status: FAILURE!`. File restored from saved copy; `diff` empty; suite back to `39 | 39 passed`.

## 4a. Game session + audio

Pure C++20 library `baccarat_game` (`src/game/`, namespace `bac::game`, zero SDL): `Session` round state machine and `parseScript` for `--script`. Suite: `game_tests` (doctest). SDL_mixer 3 `SfxBank` in `src/app/Audio.{hpp,cpp}`.
Run on 2026-10-01 (macOS arm64, Apple clang, Debug).

- [x] **Logic build**: `cmake -S baccarat-sdl -B baccarat-sdl/build-logic -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBACCARAT_BUILD_GAME=OFF && cmake --build baccarat-sdl/build-logic` → 0 warnings
- [x] **ctest (logic)**: `ctest --test-dir baccarat-sdl/build-logic --output-on-failure` → `100% tests passed out of 3` (rules_tests, game_tests, app_options_tests)
- [x] **doctest direct**: `baccarat-sdl/build-logic/src/game/game_tests` → `test cases: 23 | 23 passed`, `assertions: 676 | 676 passed`
- [x] **Phase flow + wrong-phase**: every transition, every wrong-phase call returns false / `WrongPhase` with no state change
- [x] **Bets**: chip validation, place/remove-last/removeAll/clear, bankroll + on-table conserved, all-in (`AllIn`, remainder placed), `InsufficientFunds`, rebet (`Placed`, `Partial`, `NothingToRebet`, `TableNotEmpty`)
- [x] **Rounds (stacked shoes)**: Player win (+$100 on $100), Banker win ($100 → +$95 net), Tie (P/B push, Tie 8:1), loss, history cap 30, counts, isBroke, resetSession (same shoe again), reshuffle flag at the cut card
- [x] **Script parser**: every command, whitespace, colon in paths, bad tokens throw `std::invalid_argument` naming the token
- [x] **Full build**: `cmake -S baccarat-sdl -B baccarat-sdl/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBACCARAT_BUILD_TESTS=ON && cmake --build .../build && ctest --test-dir .../build --output-on-failure` → `100% tests passed out of 5` (smoke, smoke_screenshot, rules_tests, game_tests, app_options_tests); `Audio.cpp` compiles without warnings
- [x] **SfxBank runtime**: verified by the reviewer (see Reviewer verification below): dummy audio loads all 6 sounds; null mixer or missing files → silent no-op, no crash (one warning per missing file)

### 4a Reviewer verification (2026-10-01)

- [x] `ctest --test-dir build-logic` → `100% tests passed out of 3`; `build-logic/src/game/game_tests` → `test cases: 23 | 23 passed`, `assertions: 676 | 676 passed`
- [x] Code read: phases and wrong-phase guards, settle() via `bac::rules::settle`, bankroll credited only by `returnedCents` at `collect()`, reshuffle at deal, history cap 30, no SDL and no mutable globals in `src/game`
- [x] Mutation (`allIn = false` in `placeBet`) → `test_session.cpp:130: ERROR: CHECK( all.status == Status::AllIn ) is NOT correct!`, `assertions: 676 | 672 passed | 4 failed`; restored, diff clean, rerun `676 | 676 passed`
- [x] SfxBank API check against SDL_mixer 3 header: `MIX_LoadAudio(mixer,path,bool)`, `MIX_PlayAudio(mixer,audio)`, `MIX_DestroyAudio` all match
- [x] SfxBank runtime (throwaway program, `SDL_AUDIO_DRIVER=dummy`, real assets): `sfx 0..5 loaded=1`; missing dir `loaded=0`, play() no crash; null mixer `loaded=0`, play() no crash; `done`
- Note: a missing asset dir logs one warning per file (6 lines), not one total. Harmless. The mixer must outlive the SfxBank (destroy bank first).

## 3. Table UI

`TableScene` plus SDL-free helpers in `src/app/`: `Layout` (rects, hit-testing), `Format` (money, banner, net line), `DealAnimator` (deal timeline), `Gfx` (text/image cache, rounded rects). Run on 2026-10-01.

### Automated
- [x] `cmake --build baccarat-sdl/build` → 0 warnings (all app sources recompiled)
- [x] `ctest --test-dir baccarat-sdl/build --output-on-failure` → `100% tests passed out of 10`
- [x] `build/src/app/app_ui_tests` → `test cases: 18 | 18 passed`, `assertions: 150 | 150 passed` (money formatting, banner/net text, hit-testing of spots/remove controls/chips/buttons, modal broke overlay, no overlaps, all inside 1280x720, deal timeline stages, flip scale, third-card pause, ease-out)
- [x] `script_round`: `--seed 7 --script "chip:25,bet:player,bet:banker,deal,wait:400,assert-phase:betting,quit"` → passes (full round returns to Betting)
- [x] `script_funds`: nine $100 bets, three $25 bets, $100 chip with $25 left (all-in), then bet with $0 (insufficient), then remove → bankroll asserts 10000 / 2500 / 0 / 0 / 2500 pass
- [x] `script_many_rounds`: three rounds with `next` and `rebet` → passes
- [x] `script_bad_assert`: `assert-bankroll:1` exits 4 (test uses WILL_FAIL) → passes
- [x] `cmake --build build-logic` (BACCARAT_BUILD_GAME=OFF) + ctest → `100% tests passed out of 4` (incl. `app_ui_tests`)

### Interactive checklist (headless dummy drivers; screenshots inspected with the Read tool)
Shots dir: `/tmp/baccarat/shots/`
- [x] Betting screen, chips on all three spots: `--seed 7 --script "chip:25,bet:player,chip:100,bet:banker,bet:banker,chip:5,bet:tie,bet:tie,shot:.../1_bet.bmp,quit"` → `1_bet.png`: top bar, three spots with payout labels 1:1 / 8:1 / 1:1 (5% comm.), chip stacks, totals, "-" controls, chip tray with $5 raised and ringed, DEAL/CLEAR/REBET enabled, empty card slots.
- [x] Mid-deal: `...deal,wait:40,shot:.../2_mid.bmp` → `2_mid.png`: first Player card face up with badge 4, a Banker card sliding face down, shoe stack at top right, buttons disabled.
- [x] Result banner with 3rd cards: `...deal,wait:40,next,shot:.../3_result.bmp` → `3_result.png`: 3 cards per hand, "PLAYER WINS 4 to 0", "-$180.00", Player spot gold-highlighted, winner badge gold.
- [x] Banker win with commission: `--seed 2 --script "chip:100,bet:banker,deal,next,shot:.../9_banker_2.bmp,quit"` → `9_banker_2.png`: "BANKER WINS 5 to 2", "+$95.00 (after $5.00 commission)".
- [x] History strip after ~13 rounds: `--seed 11` script of 12 rounds then a 13th → `4_history.png`: tiles P/B coloured, newest at right with gold ring, counts "P 7 B 6 T 0".
- [x] Broke overlay: ten $100 Tie bets per round until bankroll 0 → `5_broke.png`: dimmed table, "Out of chips", "New session ($1,000)" button.
- [x] All-in toast: `6_allin.png` shows "All-in: $50.00 placed" (log line `toast: All-in: $50.00 placed`); log shows `toast: Not enough funds` and `toast: Place a bet first` (`7_insufficient.png`, `8_nobet.png` saved, toast text confirmed from log only).
- [ ] Left-click on a spot / chip / button, right-click remove, "-" control, hover highlight — MANUAL — pending user
- [ ] Keyboard (1-4, P/B/T, Shift+P/B/T, Space/Enter, Backspace, R, M, Esc) — MANUAL — pending user (script calls the same action functions; key mapping itself not exercised)
- [ ] Sound effects audible (card slide/flip, chips, win/lose) and mute toggle — MANUAL — pending user (dummy audio only: no crash)
- [ ] Window resize / letterbox and smoothness at real 60 Hz on a GPU renderer — MANUAL — pending user
- [ ] "Shuffling new shoe" toast (needs a shoe past the cut card, ~70 rounds) and "New session" button click — MANUAL — pending user

### Reviewer verification (2026-10-01, independent run, headless dummy drivers)
Evidence dir: `/tmp/baccarat/ui-review/`
- `cmake --build build` → `ninja: no work to do`; `ctest --test-dir build --output-on-failure` → `100% tests passed out of 10` (smoke, smoke_screenshot, rules_tests, game_tests, script_round, script_funds, script_bad_assert, script_many_rounds, app_options_tests, app_ui_tests all Passed)
- `build/src/app/app_ui_tests` → `test cases: 18 | 18 passed`, `assertions: 150 | 150 passed`
- `cmake --build build-logic && ctest --test-dir build-logic` → `100% tests passed out of 4`
- [x] Spots/remove/clear: `a1.png` (P25 B200 T5, bankroll 770 + table 230), `a2.png` after `remove:banker` (870 + 130), `assert-bankroll:100000` after `clear` passes (exit 0)
- [x] Full rounds, seeds 1-6 (`r1..r6.png`) and 40 more seeds (`t/sheet.png`, seeds 20-59 with P$100/B$25/T$5): card order, totals (A=1, face=0, mod 10), third-card decisions (hand-checked ~14 incl. naturals, P-stands/B-draws, P3-dependent B draws, tie), banner winner, net (P win +$70.00, B win -$81.25 with $1.25 commission, Tie +$40.00 with P/B pushed) all correct. Seed 1 bankroll after payout `$1,170.00` = 770 + 400 (checked in `r1b.png`).
- [x] All-in / refusal: `d1.png` toast "All-in: $25.00 placed", `d.log` `toast: Not enough funds` at $0, asserts 10000/2500/0 pass
- [x] Broke overlay: `br2.png` "Out of chips" + "New session ($1,000)"
- [x] "Place a bet first": `e0.png`; Rebet after a round: `e1.png` ($25 re-placed, bankroll $1,000); history newest at right with ring, counts P4 B2 T0 match tiles: `e2.png`
- [x] Mid-deal frames: `m20.png` (card edge-on, flipping), `m35.png` (face-down card sliding), `m65.png` (mixed)
- [x] Code audit: no `% 10`, `8:1`, `0.95`, `5%` literals in `src/app`; labels from `bac::rules::payoutLabel`; `App::~App` resets the scene (and its SfxBank) before the mixer; TextCache bounded at 600 entries and freed in its destructor.
- Defects found: see reviewer report (REBET always enabled; one-frame badge before card at flip midpoint; `shot:` twice in one frame loses the first shot).

### 3. Table UI — reviewer fixes (2026-10-01)
- [x] REBET greyed unless Betting, table empty and a previous bet exists (`shots/f1_a.png` grey with bets, `shots/f3_rebet_on.png` blue after clear)
- [x] Hand total counts a card only when its face is drawn at least 1 px wide; animator `faceUp` is now `p > 0.5` (not visually re-shot at the exact flip frame)
- [x] Several `shot:` in one frame all save (`f1_a.bmp`, `f1_b.bmp` both written)
- [x] `$1` chip label is dark with a light outline (`shots/f1_a.png`)
- [x] `ctest` in build: `100% tests passed out of 10`

## 5. Integration (lead, clean build 2026-10-01)

- [x] `rm -rf build build-logic` then configure + `cmake --build build` → success
- [x] `ctest --test-dir build --output-on-failure` → `100% tests passed out of 10`
- [x] Suites: rules_tests 39 cases / 23,840 assertions; game_tests 23 / 676; app_ui_tests 18 / 150; app_options_tests 9 / 18 — 0 failed
- [x] `cmake -S . -B build-logic -DBACCARAT_BUILD_GAME=OFF` build + ctest → `100% tests passed out of 4`
- [x] Smoke (dummy video + audio) passes as ctest `smoke`
- [ ] MANUAL — pending user: items listed unticked in section 3
