# TESTS — mc-randomizer

Two layers:

1. **Static tests** (run anywhere, no Minecraft): `python3 -m pytest tests -q` (or `python3 -m unittest discover -s tests`)
2. **In-game checklist** (below). These need a real Bedrock 1.21.x client. No Minecraft install
   was available during the build, so every item is **MANUAL — pending user**. None are ticked.

## Static test map

| Area | Test class | What it proves |
|---|---|---|
| Engine | `TestEnginePython` | Fixed seed → same mapping. Different seeds differ. Bijection. Zero identity remaps over 12 seeds. Quantity split keeps the count. |
| Parity | `TestEngineParity` | The JS engine the pack runs gives the exact same mapping as the Python port. 7 domains listed. Dragon in the exclusion list. |
| Universe | `TestItemUniverse` | Every target id exists in `@minecraft/vanilla-data` 1.21.0. Creative-only / technical items are out. |
| Coverage | `TestDomainCoverage` | Each of the 7 domains has a real hook or data override. No domain is handled twice. |
| Overrides | `TestDataOverrides` | Every generated recipe / loot / trade file changes every output item (no identity), keeps counts, and leaves inputs alone. |
| Dragon | `TestDragonExcluded` | The runtime guard exists. No dragon data override exists. |
| Runtime hooks | `tests/test_runtime_hooks.py`, `tests/test_fishing_hook.py` | Real `main.js` + hooks run in node against a mock `@minecraft/server`. Block, mob and fishing drops are remapped with the same count. Dragon, player, tossed, loaded and own-spawned items are left alone. |
| Package | `TestPackage` | `tools/build.py` passes. The `.mcaddon` unpacks to one valid behavior pack (UUID v4s, `min_engine_version` 1.21, `@minecraft/server` 1.11.0, entry file present). No dev files leak in. |
| Types | `TestTypeCheck` | `tsc --checkJs` passes against the real `@minecraft/server` 1.11.0 typings. Every API call exists in that version. |

## Test world setup

1. Install: double-click `dist/mc-randomizer.mcaddon` (or copy the `mc-randomizer_BP` folder into
   `development_behavior_packs`).
2. Create a new world: **Creative**, **Flat**, cheats **on**.
3. World settings → Behavior Packs → activate **mc-randomizer**.
4. Experiments: none needed (the pack uses only stable APIs).
5. Enter the world. Chat should show `[mc-randomizer] seed <n>`.
6. Run `/scriptevent mcr:info`. Note the seed and the sample mappings it prints.
7. Run `/gamemode survival` for the drop tests (Creative breaks drop nothing).

## In-game checklist

Each check must give a **changed, non-identity** item. Record what you got.

### Runtime domains (seeded per world)

- [ ] **MANUAL — pending user** · Block drops: break **dirt** with a hand. You get the item
  `/scriptevent mcr:info` lists for `minecraft:dirt`, not dirt.
- [ ] **MANUAL — pending user** · Block drops are deterministic: break a second dirt block. Same item
  as the first.
- [ ] **MANUAL — pending user** · Block drop quantity: break a block that drops several items (e.g.
  a redstone ore or glowstone). Count stays in the vanilla range; item is changed.
- [ ] **MANUAL — pending user** · Container break: place a chest, put 1 stick inside, break it. The
  stick comes back as a **stick** (contents are not remapped). The chest item itself is remapped.
- [ ] **MANUAL — pending user** · Mob drops: kill a **sheep** (`/summon sheep`). Wool and mutton come
  out as their mapped items, not wool/mutton.
- [ ] **MANUAL — pending user** · Mob drops deterministic: kill a second sheep. Same mapped items.
- [ ] **MANUAL — pending user** · Player tosses an item (Q) next to nothing broken: the item is
  **not** changed.
- [ ] **MANUAL — pending user** · Fishing: fish in water until a catch. The caught item is not the
  vanilla catch shown in the mapping (e.g. raw cod → its mapped item).
- [ ] **MANUAL — pending user** · Persistence: leave and re-enter the world. `/scriptevent mcr:info`
  shows the same seed and mappings.
- [ ] **MANUAL — pending user** · New world → different seed → different mappings.

### Data-driven domains (fixed build seed)

- [ ] **MANUAL — pending user** · Crafting: craft a **furnace** (8 cobblestone). The result is not a
  furnace. It matches `furnace` in `build/remap_report.json`.
- [ ] **MANUAL — pending user** · Crafting: craft sticks and a crafting table. Both results changed.
- [ ] **MANUAL — pending user** · No duplicate vanilla recipe: the recipe book shows one result per
  pattern (the override replaced vanilla, it did not add a second recipe).
- [ ] **MANUAL — pending user** · Smelting: smelt raw iron in a furnace. The output is not an iron
  ingot.
- [ ] **MANUAL — pending user** · Chest loot: find a dungeon (or
  run `/loot spawn ~ ~ ~ loot "chests/simple_dungeon"`). The items are the mapped ones in
  `build/remap_report.json`, not vanilla dungeon items.
- [ ] **MANUAL — pending user** · Trades: `/summon villager_v2` and give it a job block (e.g. a
  composter → farmer). The items it **gives** are changed. The items it **wants** are vanilla.

### Exception

- [ ] **MANUAL — pending user** · Ender dragon: kill the dragon (`/summon ender_dragon` in the End).
  The egg, portal and XP behave as vanilla. No item is remapped near it.
