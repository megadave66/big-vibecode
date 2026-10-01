# CONTEXT — Minecraft Bedrock Randomizer Add-on

## Glossary
- **Randomizer domain**: One of three remap spaces: block drops, crafting results, mob drops.
- **Block drops**: Breaking a block yields a remapped item (e.g. dirt → diamond). Mapped to **any item**.
- **Crafting**: Crafting recipe X yields a **uniformly random item** as its result.
- **Mob drops**: Loot-table outputs remapped; quantity preserved.
- **Dragon exception**: The ender dragon's drops are untouched; everything else randomizes.

## Decisions
- Deterministic 1:1 remaps seeded per-world; not per-event RNG.
- Built as a Bedrock behavior pack (.mcaddon), remaps generated at pack build time by a script.
