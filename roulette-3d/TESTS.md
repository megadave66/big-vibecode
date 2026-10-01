# TESTS — Roulette 3D

Godot 4.7.2 stable, gdUnit4 6.2.1. Run every command from the project root:
`roulette-3d`.

A box is ticked only when a real command run showed it. The last full run was on 2026-10-01.

## Commands

| What | Command |
|---|---|
| Full suite (headless) | `tools/run_tests.sh` |
| One suite | `tools/run_tests.sh -a res://tests/<name>_test.gd` |
| Physics sim, N spins | `godot --headless --path . --fixed-fps 120 -s res://tools/sim_spins.gd -- --spins=500 --parallel=50` |
| Export macOS build | `tools/export_macos.sh` → `build/macos/Roulette3D.zip` |
| Run the exported build (smoke) | `tools/verify_build.sh` |
| Play one round + screenshots | `godot --path . -s res://tools/autoplay_shots.gd -- --out=<dir>` |
| Table / wheel screenshot | `godot --path . -s res://tools/screenshot.gd -- --scene=res://scenes/main.tscn --out=<png> --frames=30 --view=table` |
| Wheel preview shots | `godot --path . -s res://tools/wheel_preview.gd` (see the file header) |
| Bet UI preview shots | `godot --path . -s res://tools/bet_ui_preview.gd -- --mode=stacks\|win\|anim` |

You can ignore one line in test runs: `ERROR: Remote Debugger: Unable to connect to host '127.0.0.1:0'`. gdUnit4 causes it on purpose.

## Automated suites (last full run: 18/18 suites, 159/159 cases, 0 failures, 0 orphans, exit 0)

- [x] `core_contract_test.gd` — 6. Real American pocket order (38 unique; 0 opposite 00; colours alternate; n opposite n+1), colours, labels, payout table.
- [x] `bet_book_test.gd` — 4. Chip stacking, removal, totals.
- [x] `bet_layout_test.gd` — 25. 161 spots; counts per type; number sets; id format; hit zones; every spot reachable; no overlaps.
- [x] `review_bet_layout_test.gd` — 6. Independent rebuild of the whole spot table from table rules.
- [x] `resolver_test.gd` — 22. Every bet type wins and loses, including 0/00, trios, five-number on 0/00/1/2/3, and dozen/column edges.
- [x] `bankroll_test.gd` — 7. Start $1,000; no overdraw; signals.
- [x] `round_flow_test.gd` — 12. Betting → locked → settled → resolved; "no more bets" rejects edits; multi-round bankroll math; zero bankroll.
- [x] `review_flow_test.gd` — 9. Every layout spot × every number 0..37 with the real BetLayout; no odds hard-coded outside `payouts.gd`.
- [x] `table_layout_mapper_test.gd` — 3. Layout↔world round trip.
- [x] `table_input_test.gd` — 5. Mouse ray vs felt plane; main scene node names.
- [x] `bet_ui_test.gd` — 20. Chip breakdown, stacks at spot centres, hover, tooltip odds from Payouts, HUD signals, resolution signal never synchronous.
- [x] `review_bet_ui_test.gd` — 6. Rebuild leaks, mouse filters, focus, history colours.
- [x] `wheel_pocket_math_test.gd` — 12. Angle → pocket for all 38, wrap-around, rotor offsets, built ring order.
- [x] `review_wheel_test.gd` — 5. Labels vs POCKET_ORDER at 5 rotor angles; rotor CCW and ball CW; 684 still-ball placements with 0 mismatches.
- [x] `wheel_settle_test.gd` — 1 (200 real physics spins on 50 wheels). 200/200 settled in exactly one pocket; 0 timeouts; ball at rest on a pocket floor; 38/38 pockets; mean settle 7.34 s; chi² 37.9 (5% critical 52.2).
- [x] `audio_manager_test.gd` — 7. Bus volumes, every SFX file exists, unknown name safe, settings persist, ball-roll stream loops.
- [x] `screens_test.gd` — 6. Settings and how-to-play build; the rules text has every bet type with its odds from Payouts; sliders move bus volumes.
- [x] `integration_main_test.gd` — 3. Main scene, real bets, real physics spin. The wheel's pocket result feeds RoundFlow, and the bankroll matches the Resolver. A spin with no bets works. Left/right click places and removes chips.

## Physics settle invariants (sim tool)

- [x] 500 spins (`--spins=500 --parallel=50`): 500 settled, 0 timeouts, 0 outside ring, 38/38 pockets, mean 7.30 s (6.47–8.33), chi² 44.2 (< 52.2).
- [x] The section 2 agent ran 2,000 spins: 2,000 settled, 0 timeouts, chi² 33.5. The reviewer ran 100 spins with 0 timeouts.
- [x] No rigging. The reviewer audited `roulette_wheel.gd`: the ball is placed only at launch/reset, and the number is read only after 0.5 s at rest with the rotor. With the settle aid off, all 100 pockets were the same.
- Note: fairness needs a random rotor angle at launch. The rotor never stops, so it is random. With `--fixed-rotor`, chi² is 185.6.
- Note: two separate processes with the same seeds give bit-identical results. Inside one process, a re-run of a seed matches only about 10% of the time, because the physics is chaotic.

## Export

- [x] `tools/export_macos.sh` → `Export OK: build/macos/Roulette3D.zip (59828813 bytes)`. No script errors in `build/export.log`.
- [x] `tools/verify_build.sh` → `Build run OK: Roulette 3D ready: 161 bet spots, bankroll 1000; 600 frames headless, exit 0`. That means the exported binary loads the main scene.

## Visual checks (screenshots inspected with the Read tool)

- [x] Table view: layout legible; red/black correct; 0 beside 1–2, 00 beside 2–3; HUD bars clear of the layout.
- [x] Wheel view: real American order clockwise from above (0, 28, 9, 26, 30 …); numbers legible; frets, diamonds and ball visible.
- [x] Settled: the ball rests in the reported pocket (28, then 21), with the pocket highlighted and a banner such as "21 Red".
- [x] Resolution: losers swept; winners get matched payout chips; winning spots marked. Bankroll checked: $1,000 − $155 + $125 = $970.
- [x] Settings and how-to-play screens readable at 1600×900.

## Manual — pending user

- [ ] MANUAL — pending user: play several rounds with mouse and keyboard in the editor (F5) or the exported app. Left-click place, right-click remove, Space spin, Tab/C view, Esc closes screens.
- [ ] MANUAL — pending user: listen to the audio (ambient murmur, ball roll loop, fret clicks, chips, win/lose). It was never checked by ear.
- [ ] MANUAL — pending user: open the exported `.app` by double-click. It is unsigned, so macOS Gatekeeper may need right-click → Open.
- [ ] MANUAL — pending user: judge the ball's motion by feel. The bounce phase is about 1.8 s, shorter than a real wheel's 3–5 s.
