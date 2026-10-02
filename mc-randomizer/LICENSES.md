# Licenses

## Project code

All code written for this project: **No license declared yet — owner to choose**.

This includes:
- `behavior_pack/scripts/` (JavaScript runtime code)
- `tools/` (Python build and generator tools)
- Generated data files (`item_universe.js`, recipes, loot tables, trades)

## Third-party dependencies

### Shipped in the add-on

- **No art, sound or third-party code.**
- **Mojang vanilla data (derived).** The generated files in `behavior_pack/recipes/`,
  `behavior_pack/loot_tables/` and `behavior_pack/trading/` are copies of Mojang's vanilla
  Bedrock data files with the output items changed. They ship in the `.mcaddon`. See
  "Vanilla data derived files" below for the source and terms.

### Development dependencies (not shipped)

These packages are used during build and testing only, and are not included in the final `.mcaddon` file.

| Package | Version | License |
|---------|---------|---------|
| @minecraft/server | 1.11.0 | MIT |
| @minecraft/vanilla-data | 1.21.0 | MIT |
| @minecraft/common | 1.3.0 | MIT (transitive) |
| TypeScript | 5.9.3 | Apache-2.0 |

## Vanilla data derived files

Source: https://github.com/Mojang/bedrock-samples, tag `v1.21.0.3`.
Each copied file and its upstream git blob sha is listed in `data/vanilla/_raw/SOURCES.json`.

- `data/vanilla/_raw/` — byte-exact copies of the upstream files (not shipped).
- `data/vanilla/{recipes,loot_tables,trading}/` — the same files with legacy alias ids rewritten
  to current ids (not shipped).
- `behavior_pack/{recipes,loot_tables,trading}/` — generated overrides with randomized outputs
  (**shipped**).
- `build/remap_report.json` — the build-time mapping report (not shipped).

License: bedrock-samples has no OSI license. GitHub lists it as "Other". Its
[LICENSE.md](https://github.com/Mojang/bedrock-samples/blob/main/LICENSE.md) ties use to the
Minecraft EULA and Mojang's usage guidelines. Add-ons that reuse vanilla data are normal practice,
but the owner should read those terms before public distribution.

`data/vanilla_items_1.21.0.json` is not from bedrock-samples. It is extracted from the
`@minecraft/vanilla-data` 1.21.0 npm package (MIT), and is not shipped.
