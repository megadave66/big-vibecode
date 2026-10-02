#!/usr/bin/env python3
"""Generate the item universe for the remap engine.

1. Read MinecraftItemTypes values from @minecraft/vanilla-data 1.21.0
   (dev/node_modules/.../lib/mojang-item.d.ts, else lib/index.js).
   Write data/vanilla_items_1.21.0.json (sorted). If the package is missing,
   reuse the existing snapshot JSON.
2. Load data/item_exclusions.json ({id: reason}, hand-vetted). Pattern rules
   below catch new ids of known excluded kinds; any match not yet listed is
   added to the exclusions file with the rule's reason, so the file always
   lists every excluded id.
3. universe = snapshot - exclusions. Write data/item_universe.json and
   behavior_pack/scripts/item_universe.js.

Stdlib only.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
VD_LIB = ROOT / "dev" / "node_modules" / "@minecraft" / "vanilla-data" / "lib"
SNAPSHOT = DATA / "vanilla_items_1.21.0.json"
EXCLUSIONS = DATA / "item_exclusions.json"
UNIVERSE = DATA / "item_universe.json"
UNIVERSE_JS = ROOT / "behavior_pack" / "scripts" / "item_universe.js"

ID_RE = re.compile(r"^minecraft:[a-z0-9_.]+$")

# (regex on the id without namespace, reason). Safety net for ids not yet
# listed by hand in item_exclusions.json.
PATTERN_RULES = [
    (r".*_spawn_egg$", "Spawn egg; creative only"),
    (r"^spawn_egg$", "Spawn egg (generic legacy id); creative only"),
    (r".*command_block.*", "Command block; creative/operator only"),
    (r"^light_block(_\d+)?$", "Technical/creative-only block; not obtainable in survival"),
    (r".*_double_slab$", "Double slab block; not obtainable as item"),
    (r"^element_.*", "Education Edition chemistry item; not in survival"),
    (r"^compound.*|.*chemistry.*|^lab_table$|^material_reducer$|^compound_creator$|^element_constructor$",
     "Education Edition chemistry item; not in survival"),
    (r"^hard_.*", "Education Edition item; not in survival"),
    (r"^colored_torch.*", "Education Edition item; not in survival"),
    (r"^(camera|glow_stick|balloon|sparkler|medicine|ice_bomb|bleach|rapid_fertilizer)$",
     "Education Edition item; not in survival"),
    (r"^underwater_(tnt|torch)$", "Education Edition item; not in survival"),
    (r"^(chalkboard|board|portfolio|slate|poster)$", "Education Edition item; not in survival"),
    (r"^(info_update|info_update2|unknown|reserved6|client_request_placeholder_block|moving_block|piston_arm_collision|sticky_piston_arm_collision)$",
     "Technical/creative-only block; not obtainable in survival"),
    (r".*portal$", "Portal block; not obtainable"),
    (r"^(fire|soul_fire|water|lava|flowing_water|flowing_lava)$", "Fluid/fire block; not an obtainable item"),
    (r"^infested_.*|^monster_egg$", "Infested block; Silk Touch gives the normal block; not obtainable"),
    (r"^knowledge_book$", "Creative/command-only item"),
    (r"^debug_stick$", "Creative/command-only item"),
]


def read_vanilla_ids() -> list[str] | None:
    for name in ("mojang-item.d.ts", "index.js"):
        p = VD_LIB / name
        if not p.exists():
            continue
        text = p.read_text(encoding="utf-8")
        if name == "index.js":
            # Restrict to the MinecraftItemTypes enum block.
            start = text.find("var MinecraftItemTypes")
            if start < 0:
                continue
            end = text.find("})(MinecraftItemTypes", start)
            text = text[start:end if end > 0 else len(text)]
        else:
            m = re.search(r"enum MinecraftItemTypes\s*\{(.*?)\}", text, re.S)
            if not m:
                continue
            text = m.group(1)
        ids = sorted(set(re.findall(r'"(minecraft:[a-z0-9_.]+)"', text)))
        if ids:
            return ids
    return None


def main() -> int:
    ids = read_vanilla_ids()
    if ids is None:
        if not SNAPSHOT.exists():
            print("ERROR: vanilla-data missing and no snapshot JSON", file=sys.stderr)
            return 1
        print(f"vanilla-data not found; reusing {SNAPSHOT.relative_to(ROOT)}")
        ids = sorted(set(json.loads(SNAPSHOT.read_text(encoding="utf-8"))))
    else:
        DATA.mkdir(parents=True, exist_ok=True)
        SNAPSHOT.write_text(json.dumps(ids, indent=2) + "\n", encoding="utf-8")
        print(f"snapshot: {len(ids)} ids -> {SNAPSHOT.relative_to(ROOT)}")

    excl: dict[str, str] = {}
    if EXCLUSIONS.exists():
        excl = json.loads(EXCLUSIONS.read_text(encoding="utf-8"))

    added = []
    for i in ids:
        if i in excl:
            continue
        short = i.split(":", 1)[1]
        for pat, reason in PATTERN_RULES:
            if re.fullmatch(pat, short):
                excl[i] = reason
                added.append(i)
                break
    if added:
        print(f"pattern rules added {len(added)} exclusions: {added}")
    EXCLUSIONS.write_text(json.dumps(dict(sorted(excl.items())), indent=2) + "\n", encoding="utf-8")

    stale = sorted(k for k in excl if k not in set(ids))
    if stale:
        print(f"note: {len(stale)} exclusions not in snapshot (harmless): {stale}")

    universe = sorted({i for i in ids if i not in excl})
    bad = [i for i in universe if not ID_RE.match(i)]
    if bad:
        print(f"ERROR: invalid ids: {bad}", file=sys.stderr)
        return 1

    UNIVERSE.write_text(json.dumps(universe, indent=2) + "\n", encoding="utf-8")
    body = ",\n".join(f"  {json.dumps(i)}" for i in universe)
    UNIVERSE_JS.parent.mkdir(parents=True, exist_ok=True)
    UNIVERSE_JS.write_text(
        "// GENERATED by tools/gen_item_universe.py — do not edit\n"
        "/** @type {string[]} */\n"
        f"export const ITEM_UNIVERSE = [\n{body}\n];\n",
        encoding="utf-8",
    )
    print(f"universe: {len(universe)} ids (excluded {len(excl)}) -> "
          f"{UNIVERSE.relative_to(ROOT)}, {UNIVERSE_JS.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
