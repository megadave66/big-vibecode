# Handoff — Roulette 3D

This is the current state and how to resume. Godot 4.7.2 stable, GDScript, Jolt physics. Project root:
`roulette-3d`.

## What exists

- A playable 3D American roulette table (38 pockets: 1–36, 0, 00). The ball is a real rigid body, and nothing rigs the result.
- The full American bet layout has 161 spots:
  - 38 straights.
  - 62 splits (including 0-00, 0-1, 0-2, 00-2 and 00-3).
  - 12 streets, 3 trios, 22 corners, the five-number bet and 11 six-lines.
  - Dozens, columns, red/black, even/odd and low/high.
- Chips of 1, 5, 25 and 100 stack on a spot. Left-click places one; right-click removes the top chip. The bankroll is $1,000 per session, with no table minimum.
- A round goes: betting → SPIN (Space or button) → "No more bets" (locked) → ball settles → winning number banner + pocket and spot highlights → losers swept and winners paid with matched chips → bankroll updated → next round.
- Settings screen (Master/SFX/Ambient volume, saved to `user://settings.cfg`) and a how-to-play screen.
- A camera rig with a table view and a wheel view. Tab or C toggles them. The camera moves to the wheel during a spin.

## Open, run, test, export

| Task | Command |
|---|---|
| Open in editor | `godot -e --path .` (or open `project.godot` in Godot 4.7.2) |
| Run the game | `godot --path .` |
| Full test suite (headless) | `tools/run_tests.sh` (exit 0 = green; reports in `reports/`) |
| One suite | `tools/run_tests.sh -a res://tests/<name>_test.gd` |
| Physics sim | `godot --headless --path . --fixed-fps 120 -s res://tools/sim_spins.gd -- --spins=500 --parallel=50` |
| Export macOS | `tools/export_macos.sh` → `build/macos/Roulette3D.zip` |
| Smoke-run the export | `tools/verify_build.sh` |
| One autoplayed round + screenshots | `godot --path . -s res://tools/autoplay_shots.gd -- --out=<dir>` |

Last results: 159/159 test cases in 18 suites green. The export is OK (59.8 MB zip), and the exported binary starts the main scene. See `TESTS.md`.
Export templates: only `macos.zip` + `version.txt` were installed into `~/Library/Application Support/Godot/export_templates/4.7.2.stable/`.

## Architecture

```
scenes/main.tscn  scripts/main.gd         integration only: routes signals, no rules
scripts/core/     pure RefCounted logic (unit-tested, no scene deps)
  wheel_layout.gd   WheelLayout   POCKET_ORDER (real American order, clockwise from above), colours; 00 = int 37
  payouts.gd        Payouts       BetType enum + ODDS: the ONLY odds source (resolver, tooltips, how-to-play)
  bet_spot.gd       BetSpot       id, type, numbers, label, rect (hit zone), center (chip point)
  bet_layout.gd     BetLayout     builds all 161 spots; spot_at(layout_pos) hit test
  bet_book.gd       BetBook       chip stacks per spot id
  resolver.gd       Resolver      resolve(wagers, winning) -> winners/losers/returned/net
  bankroll.gd       Bankroll      balance, debit/credit, never negative
  round_flow.gd     RoundFlow     phase machine BETTING→LOCKED→SETTLED→RESOLVED; owns Bankroll + BetBook
scripts/wheel/    RouletteWheel scene (scenes/wheel/wheel.tscn), pocket_math, geometry, ball, spin batch
scripts/table/    procedural table + painted layout; TableLayoutMapper (layout units <-> world)
scripts/input/    TableInput: mouse ray vs felt plane -> table_clicked / table_hovered (layout units)
scripts/bets/     BetOverlay (hover quad, 3D chip stacks, win marks, sweep/pay animation), ChipVisual, ChipBreakdown
scripts/ui/       Hud, settings_screen, how_to_play_screen
scripts/audio/    AudioManager (autoload "Audio"): SFX pool, ball-roll loop, procedural ambient, bus volumes
scripts/camera/   CameraRig ("table" / "wheel" views)
```

Data flow: `RouletteWheel.ball_settled(n)` → `RoundFlow.ball_settled(n)` → `RoundFlow.resolve()`. That uses `Resolver` with the odds from `Payouts`, then `BetOverlay.animate_resolution(result)`. Nothing sits between the wheel's physical pocket read and the payout.

### Bet-spot map data layout

- Layout units: 1 unit = one number cell (`CELL_SIZE` 0.08 m on the felt).
  - Number n is at column c = (n-1)/3 and row r = (n-1)%3, in the cell rect [c, c+1] × [r, r+1]. Row 0 holds 1, 4 … 34.
  - 0 sits at x [-1,0] × y [0,1.5], and 00 at x [-1,0] × y [1.5,3].
  - The columns ("2 to 1") are at x [12,13]. Dozens are at y [-1,0]; the even-money strip is at y [-2,-1].
  - Layout y maps to world −z.
- Spot ids:
  - `straight_<n>`, `split_<a>_<b>`, `street_<a>`, `trio_<a>_<b>_<c>`, `corner_<a>_<b>_<c>_<d>`.
  - `five_number`, `sixline_<a>`, `dozen_<k>`, `column_<k>`.
  - `red`, `black`, `even`, `odd`, `low`, `high`.
  - Numbers in ids are sorted, and 00 = 37.
- Hit zones come in two tiers. Inside-bet zones are 0.25 × 0.25 units (EDGE 0.125) on cell edges and corners, and they win over the cell under them. Always hit-test with `BetLayout.spot_at()`, not raw rects.
  - Streets and six-lines sit on the y=0 edge.
  - Five-number sits at (0,0). Trios sit at (0,1), (0,1.5) and (0,2).
  - The top border (y=3) has no zones.

### Physics tunables (exports on `RouletteWheel`, `scripts/wheel/roulette_wheel.gd`)

- Size and ball:
  - Bowl OUTER_RADIUS is 0.42 m. Do not scale the wheel node; it must stay Y-up under WheelAnchor.
  - The ball is a RigidBody3D, radius 9.5 mm, mass 0.03 kg (a Jolt inertia workaround), with CCD.
- Rotor: an AnimatableBody3D spinning CCW at `rotor_speed` 2.2 rad/s, ±3% (`rotor_speed_variation`, period 11 s). It never stops, so its angle at launch is random.
- Launch: CW (opposite the rotor) at 2.6–3.2 m/s (`launch_speed_min/max`), with `launch_angle_jitter` 0.3 rad.
- Surfaces: wall/track/apron/deflector/rotor/fret friction and bounce exports. There are 8 diamond deflectors and an invisible lid at 0.152 m.
- Settle:
  - The number is read after the ball co-moves with the rotor (`settle_speed` 0.04, `settle_hold_time` 0.5 s).
  - The settle aid damps only when the ball already rests on a pocket floor. It cannot change the pocket.
  - `time_limit` is 45 s. After that the wheel emits `ball_timed_out`, and main.gd relaunches with the same locked bets. No result is invented.
- Global Jolt settings in `project.godot` [physics]: 120 ticks, penetration_slop 0.001, speculative_contact_distance 0.005, bounce_velocity_threshold 0.1, max_angular_velocity 800, baumgarte 0.5, position_steps 6.
- Measured: mean settle about 7.3 s, 0 timeouts in 2,000+ spins, and a uniform distribution (chi² 33–44 vs 52.2 critical).

## Assets

- All audio comes from Kenney.nl, CC0: the Casino Audio, Impact Sounds and Interface Sounds packs. Details are in `LICENSES.md`.
- The ambient casino loop and the ball-roll loop are procedural (generated at runtime).
- Procedural fallback: no textures were downloaded. The felt, wood, chips and wheel are built from Godot primitives and materials.
- gdUnit4 6.2.1 (MIT) is in `addons/gdUnit4`. The repo moved from MikeSchulze/gdUnit4 to godot-gdunit-labs/gdUnit4, and it is the same project.

## Known gaps

- The bounce phase is about 1.8 s, shorter than a real wheel's 3–5 s.
- Inside one process, re-running the same seed repeats the pocket only about 10% of the time; separate processes are bit-identical.
- The audio was never checked by ear. The ball-launch sound is a Kenney wood impact.
- At exit, Godot prints "2 ObjectDB instances leaked" (an AudioStreamWAV and its playback). It is harmless and only appears at shutdown.
- The bankroll label updates at the start of the payout animation, not at its end.
- The "2 to 1" labels are rotated and a little cramped. Tall payout stacks can hide the cell behind them.
- The export is unsigned and not notarized, so Gatekeeper may need right-click → Open. Only the macOS template is installed.
- The settle test and integration test use real-time physics (about 30 s and 20 s).
- Manual checks are still pending; see `TESTS.md` → "Manual — pending user".
