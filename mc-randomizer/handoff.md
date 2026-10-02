# Handoff — mc-randomizer

State as of 2026-10-01. Built by a lead agent with per-section implementer and reviewer subagents.
No Minecraft install was available. Nothing has been run in the game.

## What exists

A Bedrock behavior pack that remaps items in 7 domains. The ender dragon is left alone.

- `behavior_pack/` — the pack. Script module `@minecraft/server` **1.11.0**, `min_engine_version`
  `[1, 21, 0]`. Stable APIs only, no experiments.
- `scripts/remap_engine.js` — one pure engine. FNV-1a seed hash, mulberry32 PRNG, Sattolo shuffle.
  Sattolo makes one full cycle, so no item ever maps to itself.
- `scripts/item_universe.js` — 1130 survival items. Generated from `@minecraft/vanilla-data`
  1.21.0 minus 142 vetted exclusions (`data/item_exclusions.json`, each with a reason).
- `scripts/drop_interceptor.js` + `hooks/` — runtime remapping of dropped item entities.
- `recipes/`, `loot_tables/chests/`, `trading/economy_trades/` — generated override files.
- `tools/remap_engine.py` — Python port of the engine. Tests prove it matches the JS bit for bit.
- `tools/gen_item_universe.py`, `tools/gen_data_domains.py`, `tools/build.py`.
- `tests/` — 52 static tests. `TESTS.md` — in-game checklist (all pending).

## Build, test, install

```sh
cd dev && npm install && cd ..        # once: typings + tsc (dev only, not shipped)
python3 tools/build.py                # generate, validate, type-check, zip
python3 -m pytest tests -q            # 52 tests
```

Output: `dist/mc-randomizer.mcaddon`. Double-click it, or copy `mc-randomizer_BP/` into
`development_behavior_packs`. Activate it in world settings. Then follow `TESTS.md`.
`/scriptevent mcr:info` prints the world seed and sample mappings.

Last real run: `BUILD PASSED` (105 files, 57.3 KB). `52 passed`. `tsc` exit 0.

## Mechanism per domain

The scripts API (1.11.0) cannot rewrite recipes, loot tables or trades. So the pack splits:

| Domain | Mechanism | Seed |
|---|---|---|
| block_drops | `beforeEvents.playerBreakBlock` records a source. `afterEvents.entitySpawn` catches the dropped item nearby, removes it, spawns the mapped item with the same total count. | per world |
| mob_drops | `afterEvents.entityDie` records a source (skips player and `EXCLUDED_ENTITIES` = ender dragon). Same interception. | per world |
| fishing | `beforeEvents.entityRemove` on `minecraft:fishing_hook` (plus a per-tick hook scan fallback) records a source. Same interception. | per world |
| crafting | 51 recipe files reuse the vanilla identifier to replace the vanilla recipe. Only `result` changes. Includes `minecraft:furnace`. | build |
| smelting | 14 furnace recipe overrides. Only `output` changes. | build |
| chest_loot | 20 chest loot tables overridden by path, incl. `simple_dungeon`. Only item names change; `set_count` kept. | build |
| trades | 11 trade tables (10 professions + wandering trader) overridden by path. Only `gives` items change; `wants` untouched. | build |

World seed: a uint32 stored in the world dynamic property `mcr:seed` on first load. The same world
always gives the same mappings. Build seed: `hashString("mc-randomizer-build-v1")` from
`data/build_config.json`. Both feed the same engine and item universe.

## Spec conflicts resolved

- `CONTEXT.md` says 3 domains and remaps generated at build time. `prompt.md` says 7 domains and
  world-load seeding. `prompt.md` won.
- The API forces a split anyway: runtime domains use the world seed, data domains use the build
  seed. Every world gets the same crafting, smelting, loot and trade mappings. To change them,
  edit `build_seed` and rebuild.

## Known gaps

**Not verified in game.** Every `TESTS.md` item is "MANUAL — pending user". The runtime code is
tested only in node against a mock `@minecraft/server`.

Runtime domains:
- The interceptor is a heuristic: item spawns near a recent break, death or hook removal. Nearby
  tosses inside the window (1.5 blocks / 3 ticks for blocks, 3 blocks / 25 ticks for mobs,
  3 blocks / 5 ticks for fishing) can also be remapped.
- Non-player block breaks (explosions, pistons, fire, water) are not remapped.
- Mob equipment drops are remapped with normal drops.
- Container contents are skipped by exact type and amount. Merged stacks may slip through.
- Item data is lost. Names, enchantments and durability are not carried over.
- Unstackable targets are split into stacks of 1, so the count is kept.

Data domains:
- Only 65 recipe files are overridden (51 crafting, 14 smelting). All other recipes stay vanilla.
- Recipe override by same type + identifier is documented by bedrock.dev, not by Microsoft.
  Confidence medium-high. Loot and trade override by path: high.
- Not covered: cartographer, librarian and stone_mason trades, the ruined_portal chest, and any
  chest table not in `loot_tables/chests/` (e.g. end city, bastion, woodland mansion, ancient city,
  trial chambers). They stay vanilla.
- Vanilla files use legacy ids (e.g. `appleEnchanted`, `dye` + data). `gen_data_domains.py`
  rewrites them through an explicit alias table. Some are approximations, marked
  `[representative]` (wool/carpet colours, coral blocks, tipped arrows). `map` → `filled_map` is a
  guess. `red_sand` loses its variant.
- Item-specific loot functions (enchant, potion, random aux value) are dropped from remapped
  entries. Enchanted books, potions and maps come out plain.
- Villagers that give emeralds now give other items. This breaks the economy, as the spec asks.
- Recipe-book unlock behaviour is unknown.

Item universe:
- Pinned to 1.21.0 ids. Later 1.21.x builds split legacy ids (e.g. `stone_block_slab2`,
  `stonebrick`, `skull`). New ids there are not randomized. If a legacy target id no longer
  exists, `new ItemStack` throws; the interceptor catches it and keeps the original item.
- Variant ids collapse to their base item (e.g. `sandstone` covers several variants).

Licensing: vanilla data comes from Mojang/bedrock-samples `v1.21.0.3` under the Minecraft EULA
terms, not an OSI license. See `LICENSES.md`. Project code has no license yet; the owner chooses.

## Resume

1. Run the `TESTS.md` checklist in a flat creative world. Record results there.
2. If a heuristic window misses real drops, tune `radius`/`ticks` in `hooks/blocks_mobs.js`,
   `hooks/fishing.js`, or `graceTicks` in `drop_interceptor.js`.
3. If a recipe override adds a second recipe instead of replacing vanilla, check the identifier
   against `data/vanilla/_raw/`.
