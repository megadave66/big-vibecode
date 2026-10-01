# PROMPT — Minecraft Bedrock Randomizer Add-on

You are building a Minecraft Bedrock "everything randomizer" add-on in `~/Documents/ai-projects/big-vibecode/mc-randomizer`, shipped as a `.mcaddon`.

## Project summary
A behavior pack that **randomizes item outcomes across every domain**: block drops, mob drops, crafting results, furnace smelting, chest/dungeon loot, fishing, and villager trades. Implemented with the **Minecraft scripts API** (`@minecraft/server`) so mappings are computed at world load from a per-world seed. Python is used only for the dev/packaging side.

## Domain decisions (authoritative)
- **Domains, all randomized**: block drops (break dirt → get anything), mob drops (kill a sheep → get anything), crafting results (furnace recipe → gives a uniformly random item), furnace smelting outputs, chest/dungeon loot tables, fishing catches, villager trades.
- **The sole exception: the ender dragon.** Its drops/behavior are untouched.
- **No identity remaps**: every randomized mapping MUST change the item. A full random shuffle guarantees this — verify with a test that zero identity passes exist.
- **Deterministic 1:1 remaps** seeded from the world (derive a seed from world properties via the scripts API — same world always produces the same mappings). NOT per-event RNG: breaking the same block type twice gives the same item within that world.
- **Quantity preserved** where the source quantity exists; randomization changes the *item*, not the count.
- Crafting results are a **uniformly random item** per recipe (the remap is recipe→item, shuffled once per world).
- Block drops/mob drops/loot/trades/fishing: remap the resulting item to another random obtainable item, preserving stack count.
- Target **Bedrock 1.21.x stable**; set `min_engine_version` accordingly; valid pack manifest with UUIDs.
- Item universe for remapping: all obtainable survival items with valid identifiers — the mapping generator enumerates a vetted list (data-driven, from a JSON list the pack embeds or derives).

## Implementation approach
- Behavior pack: `manifest.json` + scripts entrypoint + `scripts/main.js`.
- On world load: compute the world seed → seed a deterministic PRNG → build the item permutation → install remaps.
- Randomization mechanics: prefer **overriding loot tables and recipes via the scripts API** where supported; where the API cannot rewrite a vanilla table, use event handlers (`world.afterEvents`) to intercept and replace drops/results. Document exactly which domains use which mechanism, and flag any domain the API genuinely cannot touch in `handoff.md`'s known gaps — do not silently skip it.
- Python dev tooling: `tools/build.py` to validate JSON, run consistency checks (no identity maps, all identifiers valid), and zip the `.mcaddon`.

## Test scripts (written BEFORE implementation)
- `tests/test_remaps.py`: unpack the pack, run the mapping generator logic (ported or duplicated in Python for CI-free testing), assert: deterministic for a fixed seed, no identity remaps, all targets are valid identifiers, all listed domains covered, dragon excluded.
- In-game checklist in `TESTS.md`: set up a flat test world; verify sample mappings by breaking blocks (incl. dirt), crafting a furnace recipe, killing a sheep, checking a dungeon chest, fishing, and trading — each must yield a changed, non-identity item.

## Phase 0 — Divide & assign models
Model tiers: **easy → fastest model; hard → strongest reasoning model**. Sections:

1. **Pack scaffold + scripts wiring** *(easy)* — manifest/UUIDs, folder structure, scripts API entrypoint, world-seed derivation, deterministic PRNG.
2. **Remap engine** *(hard — strongest model)* — item universe list, full shuffle without identity passes, per-domain mapping application, quantity preservation, dragon exclusion.
3. **Domain hooks: blocks & mobs** *(medium)* — drop interception/replacement for block breaks and mob deaths.
4. **Domain hooks: crafting, smelting, loot, fishing, trades** *(hard)* — the remaining five domains with whichever mechanism each requires.
5. **Packaging & docs** *(easy)* — `tools/build.py`, validation, test world setup instructions, README + LICENSES (no third-party assets expected; note any).

## Phase 1 — Delegate to sub-agents
For EACH section:
- Spawn one **implementer sub-agent** with its spec and test obligations.
- Spawn one **reviewer sub-agent** that runs the section's **written test script**: `tests/test_remaps.py`, JSON validation, and the relevant `TESTS.md` in-game checklist items (reviewer must actually trace the code paths for each domain, not trust the implementer's claims).
- Reviewer loops with the implementer until green; real output required.

## Phase 2 — Integration & handoff
- Integrate all domain hooks under the single remap engine; one PRNG, one mapping, shared by everything.
- Run full validation: build script passes, zero identity remaps confirmed, all domains covered, dragon untouched (grep/trace for explicit exclusion).
- Write **`handoff.md`**: what exists, how to build/install the .mcaddon and test it, which mechanism each domain uses, known gaps (any domain the API can't fully touch). State + resume only — no v2 ideas.