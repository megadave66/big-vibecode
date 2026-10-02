# MC-Randomizer

A Minecraft Bedrock add-on that randomizes item outcomes across every domain.

## What it does

The add-on uses a deterministic seeded shuffler to remap items in seven different ways:

1. **Block drops**: breaking a dirt block gives a random item instead of dirt
2. **Mob drops**: killing a sheep gives random items instead of wool/mutton
3. **Crafting**: crafting-table recipes produce random items instead of the recipe output
4. **Smelting**: furnace smelting outputs random items
5. **Chest loot**: dungeon chests contain remapped items
6. **Fishing**: caught items are remapped
7. **Villager trades**: trade outputs are remapped

**Exception**: the ender dragon's drops and behavior remain unchanged.

Block drops, mob drops, and fishing use a per-world seed, so they change every world. Crafting, smelting, chest loot, and villager trades use a fixed build seed, so they are identical in every world. This is a known API limit — Bedrock scripts cannot rewrite recipes, loot tables, or trades at runtime.

## Install

### Option 1: Double-click the add-on
1. Download or build `dist/mc-randomizer.mcaddon`
2. Double-click it (or import it into Minecraft Bedrock)
3. Create a new world and activate the add-on in the world settings
4. No experiments are needed

### Option 2: Copy to development packs
1. Copy the `behavior_pack` folder and rename it to `mc-randomizer_BP`
2. Copy it to your Bedrock `development_behavior_packs` directory
3. Create a new world and activate it in the world settings

## Build and test

First install dependencies (once):

```bash
cd dev && npm install
```

Build the add-on:

```bash
python3 tools/build.py
```

Build with options:

```bash
python3 tools/build.py --check              # Validate without creating zip
python3 tools/build.py --skip-typecheck     # Skip TypeScript checks
python3 tools/build.py --no-generate        # Skip running generators
```

Run all tests:

```bash
python3 -m pytest tests -q
```

## In-game commands

```
/scriptevent mcr:info
```

Shows the current world seed and sample item mappings (dirt, white wool, mutton, cod, rotten flesh). Also reminds you that crafting, smelting, chest loot, and villager trades use the fixed build seed — see `build/remap_report.json` for exact mappings.

## Project layout

```
behavior_pack/              the behavior pack
  manifest.json
  scripts/                  the runtime code
    main.js                 entry point
    prng.js                 PRNG and hash functions
    seed.js                 world seed derivation
    remap_engine.js         the shuffle algorithm
    item_universe.js        generated: list of remappable items
    drop_interceptor.js     shared item-entity interception
    hooks/
      blocks_mobs.js        block/mob drop remapping
      fishing.js            fishing output remapping
  recipes/                  generated: crafting/smelting recipe overrides
  loot_tables/              generated: chest loot overrides
  trading/                  generated: villager trade overrides

data/                       data files
  item_universe.json        sorted list of remappable items
  item_exclusions.json      items that must not be remapped
  vanilla_items_1.21.0.json all obtainable Bedrock items
  vanilla/                  copies of vanilla recipes/loot/trades
  build_config.json         seed for data-domain remapping

tools/                      build tools
  build.py                  build and validate script
  remap_engine.py           Python port of the shuffle algorithm
  gen_item_universe.py      generates item universe
  gen_data_domains.py       generates recipe, loot, trade overrides and build/remap_report.json

build/                      generated at build time
  remap_report.json         exact mappings used in the pack

dist/                       output
  mc-randomizer.mcaddon     the final add-on file

tests/
  test_remaps.py            automated test suite
```

## How each domain works

| Domain | Mechanism | Notes |
|--------|-----------|-------|
| Block drops | Runtime script hook | Items spawn when you break a block; script intercepts and remaps them using the per-world seed |
| Mob drops | Runtime script hook | Items spawn when mobs die; script intercepts and remaps them using the per-world seed |
| Crafting | Data-driven recipe overrides | Generated recipe files override vanilla crafting-table recipes |
| Smelting | Data-driven recipe overrides | Generated furnace recipe files override vanilla smelting outputs |
| Chest loot | Data-driven loot table overrides | Generated loot files override vanilla chest contents |
| Fishing | Runtime script hook | Items spawn when fishing rod hook reels in; script intercepts and remaps them using the per-world seed |
| Villager trades | Data-driven trade overrides | Generated trading files override vanilla villager trade outputs |

**Per-world vs fixed**: Runtime domains (block drops, mob drops, fishing) use the per-world seed, so they change every world. Data domains (crafting, smelting, chest loot, trades) use the fixed build seed, so they are the same in every world — this is a known limit of the Bedrock scripts API, which cannot rewrite recipes, loot tables, or trades at runtime.

## Testing

See `TESTS.md` for the full test plan, including in-game checklist items.

Run the static test suite:

```bash
python3 -m pytest tests -q
```

## Documentation

- **TESTS.md**: test plan (automated + in-game checklist)
