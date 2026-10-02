# Level format (format version 1)

This is the authoritative object palette. The loader (`src/core/LevelLoader.cpp`) and the
validator (`src/core/Validator.cpp`, CLI `gd_validate`) both enforce it. Any object type or
field not listed here is an error.

## File
One JSON file per level: `levels/levelNN.json` (NN = 01..10).

```json
{
  "format": 1,
  "id": 1,
  "name": "First Steps",
  "difficulty": 1,
  "music": "music/track01.ogg",
  "start": { "mode": "cube", "speed": "normal", "gravity": "down" },
  "ceiling": 12,
  "colors": { "bg": "#203a8f", "ground": "#1a2f6e", "accent": "#5fd0ff" },
  "objects": [
    { "type": "spike", "x": 14, "y": 0 },
    { "type": "block", "x": 20, "y": 0, "w": 2, "h": 1 },
    { "type": "end_wall", "x": 470 }
  ]
}
```

### Top-level fields
| field | type | required | rule |
|---|---|---|---|
| `format` | int | yes | must be `1` |
| `id` | int | yes | 1..10, matches file number |
| `name` | string | yes | 1..32 chars |
| `difficulty` | int | yes | 1..10 |
| `music` | string | yes | path relative to `assets/`; file must exist (validator checks when data dir given) |
| `start.mode` | string | no | `cube` (default) or `ship` |
| `start.speed` | string | no | `slow`, `normal` (default), `fast`, `faster` |
| `start.gravity` | string | no | `down` (default) or `up` |
| `ceiling` | number | no | world ceiling height in blocks; default 12; range 6..20 |
| `colors.*` | string | no | `#rrggbb`; keys `bg`, `ground`, `accent` only |
| `objects` | array | yes | palette objects below |

Unknown top-level keys are an error. Wrong JSON types are errors (an int field given `1.0` or `"1"`
fails). The loader reports every error it finds, each with a JSON path such as
`objects[12].w: must be > 0`. When the file is named `levelNN.json`, `id` must equal NN.

## Object palette
All coordinates in blocks. `x`, `y` = bottom-left corner. `x`, `y`, `w`, `h` must be multiples
of 0.5. Every object must lie fully between the ground (y ≥ 0) and the ceiling, and at x ≥ 0.

Required fields: `type` and `x` always; `y` for `block`, `platform`, `spike`, `deco`; `gravity` /
`mode` / `speed` for the matching portal; `kind` for `deco`. Every other field has the default
shown. Loader-set sizes: `platform` h = `kPlatformHeight`, portals h = `kPortalDrawHeight`, `end_wall`
spans y = 0 to the ceiling. Objects are sorted by `x` (stable) after loading.

| type | fields | behaviour |
|---|---|---|
| `block` | `x`, `y`, `w`=1, `h`=1 | Solid. Player can stand on the surface it falls toward (top when gravity down, bottom when gravity up). Ship slides on top and bottom. Inner hitbox overlap (side hit, head hit for cube) = death. |
| `platform` | `x`, `y`, `w`=1 | Solid slab of fixed height 0.5. Same rules as `block`. |
| `spike` | `x`, `y`, `dir`=`up` | Hazard in a 1×1 cell. `dir`: `up` (points up, sits on a floor) or `down` (hangs from a ceiling). Hitbox is the small box from `PhysicsConstants.h` (`kSpikeHit*`). Any overlap with the player box = death. |
| `portal_gravity` | `x`, `y`=0, `gravity` | Sets gravity `down` or `up`. |
| `portal_mode` | `x`, `y`=0, `mode` | Sets gamemode `cube` or `ship`. |
| `portal_speed` | `x`, `y`=0, `speed` | Sets speed `slow`, `normal`, `fast`, `faster`. |
| `end_wall` | `x` | Finish line. Player reaching it (left edge + size ≥ x) wins. Exactly one per level. It is the right-most object: no other object may have `x` greater than the end wall's `x`. |
| `deco` | `x`, `y`, `w`=1, `h`=1, `kind` | Visual only, never collides. `kind`: `pillar`, `chain`, `star`, `arrow`, `glow`. |

### Portals
Portals are **column triggers**: a portal fires once, on the tick when the player's centre x
first reaches the portal's `x + 0.5`, at any height. `y` only places the drawing (the portal is
drawn 3 blocks tall from `y`). Firing the same state again (e.g. gravity down when already down)
is allowed and does nothing.

When the mode changes cube → ship the player keeps its vertical velocity (clamped to ship
limits). Ship → cube keeps velocity too.

## Level length
The player starts with its left edge at x = 0 and wins when the left edge reaches
`endX - kPlayerSize`. Speed portals fire when the left edge reaches `portal.x`.
Duration = sum over speed segments of (segment length / speed), over left-edge positions
0 .. `endX - kPlayerSize`. Must be 30..60 seconds.
Speeds (blocks per second) are in `PhysicsConstants.h` (`kSpeed*`).

## Completability invariants (validator)
The validator rejects a level that breaks any of these. Constants are from
`PhysicsConstants.h`; never hard-code numbers in the validator.

1. **Palette**: only types and fields above; all values in range; exactly one `end_wall`, right-most.
2. **No embedded hazards**: a spike may not overlap a solid.
3. **Climb limit (cube)**: in cube sections, any solid face the cube must climb rises at most
   `kCubeMaxClimb` above the highest standable surface the cube can stand on just before it.
4. **Spike-run limit (cube)**: a run of floor spikes with no safe surface between them is at
   most `kCubeMaxSpikeRun` blocks long.
5. **Ship corridor**: in ship sections, at every column the free vertical gap between obstacles
   (solids, spike hitboxes, ground, ceiling) is at least `kShipMinGap`, and the gap's centre
   moves by at most `kShipMaxSlope` blocks per block of x.
6. **Duration** 30..60 s.
7. **Start clear**: the first 8 blocks hold no hazards or solids.

### Validator algorithm
Source of truth: the comment block at the top of `src/core/Validator.cpp`. In short:
- Mode, gravity and speed are tracked by walking the portals in x order. A portal applies to a
  player whose left edge is at or past `portal.x`. Cube rules for an object at `x` use the state at
  `x - kPlayerSize` (the moment the cube touches it).
- **Climb**: for each solid, `rise = top - floor`, where `floor` is the highest surface (ground, or
  a solid that starts left of it, within half a jump length) that is not above its top. Solids
  whose underside is at least `kPlayerSize` above that floor are passed beneath and skipped.
  Stacked solids count as one wall. Limit: `kCubeMaxClimb + 0.05` (so a 2-block wall is legal).
  Gravity up mirrors y about the ceiling.
- **Spike run**: floor spikes (up for gravity down, down for gravity up) at similar heights form one
  run while the gap between hitboxes is under `kPlayerSize + 0.2`. Run size = `last.x - first.x + 1`
  must be at most `cubeMaxSpikeRun(speed)`.
- **Ship**: in ship sections, every 0.1 block the free gaps (ground, ceiling, solids, spike hitboxes)
  are found. One gap of at least `kShipMinGap` must exist (`ship-gap`). The ship centre may move at
  most `kShipMaxSlope` per block of x; the validator tracks the set of reachable centre heights and
  fails (`ship-slope`) when a column cannot be reached. Because all objects are axis-aligned, a
  corridor can only change in steps; a step is legal when the ship can already be in the new range.
- **Duration**, **start clear** (no block, platform or spike with `x < kStartClearBlocks`) and
  **music** (file exists under `<data>/assets/`) are direct checks.

Static rules can not prove a level beatable. The proof is a **replay**: `gd_solve` searches for
an input sequence; `gd_replay levels/levelNN.json replays/levelNN.replay` re-runs it headless and
exits 0 only if the player reaches the end wall without dying. ctest runs both for every level.

## Replay file format
Plain text, one item per line, `#` starts a comment.
```
gdreplay 1
level 3
tickrate 240
press 120
release 130
press 400
end 11000
```
`press T` / `release T` change the held state at the start of tick T (0-based). `end T` is the
tick count the run must reach the end wall by. Ticks must increase.

## Authoring guide (short)
- Keep the first 8 blocks clear so the player can react.
- Cube jump: apex ≈ `kCubeJumpApex` blocks, length ≈ `kCubeJumpLength(speed)` blocks.
- Use `block` with `w`/`h` for long floors or walls instead of many 1×1 blocks.
- After editing: `build/gd_validate levels/levelNN.json`, then `build/gd_solve levels/levelNN.json
  -o replays/levelNN.replay`, then `build/gd_replay levels/levelNN.json replays/levelNN.replay`.
