# Handoff — Baccarat (Punto Banco, SDL3 + C++20)

## What exists
A playable Punto Banco table. Bets: Player 1:1, Banker 1:1 minus 5% commission, Tie 8:1 (Player and Banker push on a Tie). Start bankroll $1,000. Chips $1/$5/$25/$100. 8-deck shoe, reshuffle when 52 cards remain. History strip of the last 20 results. Card slide and flip animation. Click DEAL when ready (no timer).

## Build, run, test
```
cd baccarat-sdl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
build/src/app/baccarat                      # play
```
Logic only, no SDL download: `cmake -S . -B build-logic -G Ninja -DBACCARAT_BUILD_GAME=OFF` then build and ctest (4 tests).

Dependencies come from pinned release archives with SHA256 (`cmake/Dependencies.cmake`): SDL3 3.4.16, SDL_ttf 3.2.2 (FreeType only), SDL_mixer 3.2.4 (WAV + OGG), doctest 2.5.3. Git clones of SDL failed on this machine, so archives are used. SDL_ttf's FreeType fork is fetched at the commit it pins.

Debug flags: `--frames N`, `--screenshot PATH.bmp`, `--seed N`, `--script "cmds"`. Script commands: `chip:N, bet:/remove:player|banker|tie, clear, rebet, deal, wait:N, next, shot:PATH, assert-bankroll:CENTS, assert-phase:betting|dealing|resolution|payout, quit`. A failed assert exits 4.
Headless run: `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy build/src/app/baccarat --seed 7 --script "chip:25,bet:player,deal,wait:200,shot:/tmp/x.bmp,quit"`.

Controls: click spots and chips; right-click or "-" removes; keys 1-4, P/B/T, Shift+P/B/T, Space/Enter, Backspace, R, M, Esc.

## Architecture
- `src/rules` (`baccarat_rules`, `bac::rules`): the deep module. Pure C++20, no SDL. Single include `baccarat/rules/rules.hpp`, which documents the full API.
  - Constants: payouts, commission, bankroll, chips, deck count, cut card.
  - `handTotal`, `playerShouldDraw`, `bankerShouldDraw(playerTotal, bankerTotal, playerThirdPoints)`.
  - `Shoe(seed)` (own Fisher-Yates, same order on every platform), `Shoe::stacked` for tests.
  - `playRound(Shoe&)` returns `RoundResult` with totals, outcome and the ordered deal steps.
  - `settle(Bets, Outcome)` returns `Settlement`. `payoutLabel(BetSpot)` gives UI labels.
  - Commission is floor(winnings x 5 / 100) in cents.
- `src/game` (`baccarat_game`, `bac::game`): `Session` round state machine (Betting, Dealing, Resolution, Payout), bet stacks, all-in, rebet, history, `parseScript`. No SDL.
- `src/app` (`baccarat`): SDL3 front end. `TableScene` (UI and script runner), `Layout`, `Format`, `DealAnimator` (pure, tested), `Gfx` (text cache, drawing), `Audio` (`SfxBank`). The UI only calls the rules and session code.
- Money is `int64_t` cents everywhere.

## Tests (clean build, this machine)
10 ctest targets, all pass: `smoke`, `smoke_screenshot`, `rules_tests` (39 cases, 23,840 assertions), `game_tests` (23, 676), `app_options_tests` (9, 18), `app_ui_tests` (18, 150), `script_round`, `script_funds`, `script_many_rounds`, `script_bad_assert` (expects exit 4). Details and reviewer evidence are in `TESTS.md`.

## Assets
All CC0 (Kenney) or OFL (Lato). Logged in `LICENSES.md`. The felt is drawn in code (no texture found). `card_flip` and `lose` sounds are substitutes from other Kenney sounds.

## Known gaps
- MANUAL — pending user (listed unticked in `TESTS.md` section 3): mouse clicks and hover, key mapping, audible sound, window resize and GPU frame rate, the "Shuffling new shoe" toast, clicking "New session".
- Only built and run on macOS arm64. No Linux or Windows run.
- Third card sits beside the other two, not rotated.
- Chip stacks draw at most 5 sprites; the spot total is exact.
- Software renderer in headless mode is slow (script_round takes about 7 s).
- A missing assets folder logs one warning per sound file.
