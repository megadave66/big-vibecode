# Common brief — read fully before you start (all mc-randomizer subagents)

Project root (R): `mc-randomizer`
Spec: `R/prompt.md` (authoritative), `R/CONTEXT.md` (older; where it conflicts, prompt.md wins).
Tests (the spec, already written, do not weaken them): `R/tests/test_remaps.py`, `R/TESTS.md`.
Run tests: `cd R && python3 -m pytest tests -q` (or `python3 -m unittest tests/test_remaps.py -v`).

## Hard rules
- Stay inside R. Other agents build sibling projects in `*`. Never touch them.
- NO git commands at all.
- Only edit files your section owns (listed in your brief). If you need a change elsewhere, say so in your report.
- No Minecraft install exists. Never claim an in-game check passed. TESTS.md in-game items stay "MANUAL — pending user", unticked.
- Do not download or run anything except `npm install` inside `R/dev` (already done: @minecraft/server 1.11.0, @minecraft/vanilla-data 1.21.0, typescript). Reading docs/raw JSON with WebFetch is fine (Microsoft Learn, github.com/Mojang/bedrock-samples, npm typings).
- Report honestly with real command output. Do not tick anything you did not run.
- Python: stdlib only (python3 3.14). Node is available (v26).

## Target
- Minecraft Bedrock 1.21.x stable. Script module `@minecraft/server` **1.11.0** (oldest stable for 1.21.0, broadest support). `min_engine_version` `[1, 21, 0]`.
- Typings: `R/dev/node_modules/@minecraft/server/index.d.ts`. Type-check: `R/dev/node_modules/.bin/tsc -p R/dev/tsconfig.json` (checkJs over `behavior_pack/scripts/**/*.js`). It must exit 0. Only use APIs that exist in 1.11.0. Notable: no `world.seed`, no `ItemTypes` class, no `playerInteractWithBlock`, no crafting/recipe/loot/trade scripting API. `new ItemStack(id, n)` throws on bad id; `ItemStack.maxAmount` exists. `world.afterEvents.worldInitialize` exists (1.x). `system.afterEvents.scriptEventReceive` exists. `world.beforeEvents.entityRemove`, `world.beforeEvents.playerBreakBlock`, `world.afterEvents.{playerBreakBlock, entityDie, entitySpawn}` exist. Bedrock scripts are ES modules; imports use relative paths with `.js`.

## Layout (fixed contract)
```
R/behavior_pack/                     the pack (zipped as mc-randomizer_BP/ inside the .mcaddon)
  manifest.json                      S1
  scripts/main.js                    S1  entry; wires everything
  scripts/prng.js                    S1  pure: hashString, mulberry32, randInt
  scripts/seed.js                    S1  getWorldSeed() via dynamic property "mcr:seed"
  scripts/remap_engine.js            S2  pure: DOMAINS, EXCLUDED_ENTITIES, buildPermutation, createRemapper, splitCount
  scripts/item_universe.js           S2  GENERATED from data/item_universe.json: export const ITEM_UNIVERSE = [...]
  scripts/drop_interceptor.js        S3  item-entity interception shared by runtime domains
  scripts/hooks/blocks_mobs.js       S3  export function install(ctx)
  scripts/hooks/fishing.js           S4  export function install(ctx)
  recipes/**.json                    S4  GENERATED overrides (crafting + smelting)
  loot_tables/chests/**.json         S4  GENERATED overrides (chest loot)
  trading/**.json                    S4  GENERATED overrides (villager trades)
R/data/item_universe.json            S2  sorted unique ids
R/data/item_exclusions.json          S2  {id: reason}
R/data/vanilla_items_1.21.0.json     S2  snapshot of MinecraftItemTypes values from vanilla-data 1.21.0
R/data/build_config.json             S2  {"build_seed": "mc-randomizer-build-v1"}
R/data/vanilla/{recipes,loot_tables,trading}/...   S4  vanilla source copies, same relative paths as pack
R/tools/remap_engine.py              S2  Python port (hash_string, mulberry32, build_permutation, split_count)
R/tools/gen_item_universe.py         S2
R/tools/gen_data_domains.py          S4  writes pack recipes/loot/trading + R/build/remap_report.json
R/tools/build.py                     S5  validate + checks + typecheck + zip -> R/dist/mc-randomizer.mcaddon
R/README.md, R/LICENSES.md           S5
R/handoff.md                         lead only
```

## Exact algorithms (JS and Python must match bit for bit)
- `hashString(str)` — FNV-1a 32-bit over UTF-16 code units: `h = 0x811c9dc5; for each c: h ^= c; h = Math.imul(h, 0x01000193) >>> 0;` return uint32.
- `mulberry32(seed)` returns `() => float in [0,1)`:
  ```js
  let a = seed >>> 0;
  return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  ```
- `randInt(rng, n)` = `Math.floor(rng() * n)`.
- `buildPermutation(seed, universe)` → plain object `{src: dst}`. Sort a deduped copy of universe ascending (JS default string sort = code-unit order; Python `sorted` matches for ASCII). `arr = sorted.slice(); rng = mulberry32(seed); for (i = n-1; i >= 1; i--) { j = randInt(rng, i); swap(arr[i], arr[j]); }` (Sattolo: one n-cycle, so zero fixed points). `map[sorted[k]] = arr[k]`.
- `splitCount(total, maxStack)` → list of full `maxStack` stacks then the remainder; `[]` for total <= 0.
- One permutation is shared by all 7 domains. Runtime domains (block_drops, mob_drops, fishing) use the per-world seed. Data domains (crafting, smelting, chest_loot, trades) are files on disk, so they use the build seed `hashString(build_config.build_seed)`. This split is a known limit; the lead documents it.
- DOMAINS = `["block_drops","mob_drops","crafting","smelting","chest_loot","fishing","trades"]`. EXCLUDED_ENTITIES = `["minecraft:ender_dragon"]` (also exclude `minecraft:player` from mob drops in the hook, separately).

## Runtime ctx contract (main.js builds it, hooks receive it)
```js
ctx = {
  seed,                 // uint32 world seed
  remapper,             // createRemapper(seed, ITEM_UNIVERSE): { map(id) -> id | undefined (undefined = not in universe, leave alone), mapping }
  interceptor,          // from drop_interceptor.js createInterceptor(remapper)
  log(msg),             // console.warn with "[mc-randomizer] " prefix
}
interceptor.recordSource({ domain, dimensionId, location, radius, ticks, skip })
  // "item entities that spawn within `radius` blocks of `location` in `dimensionId`
  //  within `ticks` ticks of now belong to `domain` and must be remapped".
  // skip: optional array of {typeId, amount} that must NOT be remapped (e.g. container contents).
```
The interceptor subscribes once to `world.afterEvents.entitySpawn`, buffers item entities, matches them to sources, removes the original entity and spawns the mapped item(s) with the same total count (split by the new item's maxAmount). It must ignore entities it spawned itself (no double remap).
