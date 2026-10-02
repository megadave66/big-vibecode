#!/usr/bin/env python3
"""Generate the data-driven domain overrides (crafting, smelting, chest loot, trades).

Stdlib only. Deterministic. Run from anywhere:  python3 tools/gen_data_domains.py

Inputs
  data/vanilla/_raw/<rel>        verbatim vanilla files from Mojang/bedrock-samples
                                 (tag in data/vanilla/_raw/SOURCES.json, with git blob sha
                                 + size per file; checked here on every run)
  data/item_universe.json        the item universe (S2)
  data/build_config.json         {"build_seed": "..."}  -> seed = hash_string(build_seed)

Outputs
  data/vanilla/<rel>             "normalised vanilla": the raw file with ONLY legacy item-id
                                 aliases inside remap zones rewritten to the canonical id
                                 (see ALIASES). Every other byte of meaning is the same.
                                 tests/test_remaps.py compares the pack file to this copy.
  behavior_pack/<rel>            the override: normalised vanilla with every remap-zone item
                                 replaced by mapping[item]. Recipes reuse the vanilla
                                 identifier, so they replace the vanilla recipe; loot and
                                 trade tables use the vanilla path, so they replace it.
  build/remap_report.json        {"build_seed", "seed", "domains": {domain: [{file,from,to}]}}

Remap zones (everything outside a zone is copied unchanged)
  recipes   "result" (shaped/shapeless) and "output" (furnace): the item id. "data" is
            dropped (it belonged to the old item). "count" is kept.
  loot      every entry with "type": "item": "name". Only "set_count" functions are kept
            (exactly); item-specific functions (enchant_*, set_data, set_potion, maps...)
            are dropped because they no longer fit the new item.
  trades    every item in "gives" (incl. "choice" lists). "quantity" kept. Only "set_count"
            functions are kept. "wants" is untouched.

A raw file whose zone items are not all in the universe (after aliasing) is skipped: it
keeps vanilla behaviour, and the generator lists it on stdout.
"""

from __future__ import annotations

import copy
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import remap_engine  # noqa: E402

DATA = ROOT / "data"
RAW = DATA / "vanilla" / "_raw"
NORM = DATA / "vanilla"
PACK = ROOT / "behavior_pack"
REPORT = ROOT / "build" / "remap_report.json"
SUBDIRS = ("recipes", "loot_tables", "trading")
DOMAIN_ORDER = ("crafting", "smelting", "chest_loot", "trades")

# Bedrock legacy item names still used in the 1.21.0 vanilla data files, mapped to the
# canonical 1.21 item id the game resolves them to. Only applied inside remap zones.
ALIASES = {
    "minecraft:appleEnchanted": "minecraft:enchanted_golden_apple",
    "minecraft:appleenchanted": "minecraft:enchanted_golden_apple",
    "minecraft:horsearmorleather": "minecraft:leather_horse_armor",
    "minecraft:horsearmoriron": "minecraft:iron_horse_armor",
    "minecraft:horsearmorgold": "minecraft:golden_horse_armor",
    "minecraft:horsearmordiamond": "minecraft:diamond_horse_armor",
    "minecraft:record_13": "minecraft:music_disc_13",
    "minecraft:record_cat": "minecraft:music_disc_cat",
    "minecraft:record_blocks": "minecraft:music_disc_blocks",
    "minecraft:record_chirp": "minecraft:music_disc_chirp",
    "minecraft:record_far": "minecraft:music_disc_far",
    "minecraft:record_mall": "minecraft:music_disc_mall",
    "minecraft:record_mellohi": "minecraft:music_disc_mellohi",
    "minecraft:record_stal": "minecraft:music_disc_stal",
    "minecraft:record_strad": "minecraft:music_disc_strad",
    "minecraft:record_ward": "minecraft:music_disc_ward",
    "minecraft:record_11": "minecraft:music_disc_11",
    "minecraft:record_wait": "minecraft:music_disc_wait",
    "minecraft:record_otherside": "minecraft:music_disc_otherside",
    "minecraft:record_pigstep": "minecraft:music_disc_pigstep",
    "minecraft:record_5": "minecraft:music_disc_5",
    "minecraft:record_relic": "minecraft:music_disc_relic",
    "minecraft:record_creator": "minecraft:music_disc_creator",
    "minecraft:record_precipice": "minecraft:music_disc_precipice",
    "minecraft:emptyMap": "minecraft:empty_map",
    "minecraft:emptymap": "minecraft:empty_map",
    "minecraft:carrotOnAStick": "minecraft:carrot_on_a_stick",
    "minecraft:carrotonastick": "minecraft:carrot_on_a_stick",
    "minecraft:muttonRaw": "minecraft:mutton",
    "minecraft:muttonraw": "minecraft:mutton",
    "minecraft:muttonCooked": "minecraft:cooked_mutton",
    "minecraft:muttoncooked": "minecraft:cooked_mutton",
    "minecraft:fish": "minecraft:cod",
    "minecraft:cooked_fish": "minecraft:cooked_cod",
    "minecraft:clownfish": "minecraft:tropical_fish",
    "minecraft:reeds": "minecraft:sugar_cane",
    "minecraft:speckled_melon": "minecraft:glistering_melon_slice",
    "minecraft:netherbrick": "minecraft:nether_brick",
    "minecraft:fireball": "minecraft:fire_charge",
    "minecraft:chorus_fruit_popped": "minecraft:popped_chorus_fruit",
    "minecraft:totem": "minecraft:totem_of_undying",
    "minecraft:turtle_shell_piece": "minecraft:turtle_scute",
    # Old "id:aux" form used by the trade tables (aux value = legacy data value).
    "minecraft:coal:0": "minecraft:coal",
    "minecraft:coal:1": "minecraft:charcoal",
    "minecraft:dye:4": "minecraft:lapis_lazuli",
    "minecraft:bucket:2": "minecraft:cod_bucket",
    # aux = stew effect variant (re-rolled by random_aux_value); the item is suspicious_stew.
    "minecraft:suspicious_stew:0": "minecraft:suspicious_stew",
    "minecraft:arrow:0": "minecraft:arrow",
    "minecraft:bucket:4": "minecraft:tropical_fish_bucket",
    "minecraft:bucket:5": "minecraft:pufferfish_bucket",
    "minecraft:sand:0": "minecraft:sand",
    # 1.21.0 has no separate red_sand item id: red sand is minecraft:sand (variant 1).
    "minecraft:sand:1": "minecraft:sand",
    "minecraft:tallgrass:2": "minecraft:fern",
    # Legacy Bedrock name of the filled map (the empty map is "emptyMap"). In the chest
    # tables it carries exploration_map (an explorer map), which is a filled map.
    "minecraft:map": "minecraft:filled_map",
    # Legacy "sapling" with no data value = data 0 = oak.
    "minecraft:sapling": "minecraft:oak_sapling",
}

# REPRESENTATIVE aliases (lossy, flagged in the stdout report). The vanilla entry is a
# random variant (random colour / random block state / tipped arrow) and the universe has
# no single id for "any variant". We key the remap on one representative id so the entry
# still gets a deterministic randomized result. The randomizing function itself is dropped
# from the override, so the variant choice no longer matters.
REPRESENTATIVE = {
    "minecraft:wool": "minecraft:white_wool",          # random_block_state color 0..15
    "minecraft:carpet": "minecraft:white_carpet",      # random_block_state color 0..15
    "minecraft:coral_block": "minecraft:tube_coral_block",  # random coral_color 0..4
    # Tipped arrows "arrow:<potion aux>"; tipped_arrow is not in the universe.
    **{f"minecraft:arrow:{n}": "minecraft:arrow" for n in range(1, 64)},
}
ALIASES.update(REPRESENTATIVE)


# Legacy (id, "data") pairs in recipe results -> canonical id. The normalised copy uses the
# canonical id and drops "data" (the canonical id already encodes the variant).
# Planks aux order (Bedrock legacy): 0 oak, 1 spruce, 2 birch, 3 jungle, 4 acacia, 5 dark oak.
DATA_ALIASES = {
    ("minecraft:planks", 0): "minecraft:oak_planks",
    ("minecraft:planks", 1): "minecraft:spruce_planks",
    ("minecraft:planks", 2): "minecraft:birch_planks",
    ("minecraft:planks", 3): "minecraft:jungle_planks",
    ("minecraft:planks", 4): "minecraft:acacia_planks",
    ("minecraft:planks", 5): "minecraft:dark_oak_planks",
    # Loot entries: data value comes from a set_data function.
    ("minecraft:dye", 0): "minecraft:ink_sac",
    ("minecraft:dye", 4): "minecraft:lapis_lazuli",
    ("minecraft:log2", 0): "minecraft:acacia_log",
    ("minecraft:log2", 1): "minecraft:dark_oak_log",
}


def fn_name(f) -> str:
    """Function name without the optional "minecraft:" prefix."""
    n = f.get("function", "") if isinstance(f, dict) else ""
    return n[len("minecraft:"):] if n.startswith("minecraft:") else n


def norm(i: str) -> str:
    return i if ":" in i else "minecraft:" + i


def canon(i: str) -> str:
    n = norm(i)
    return ALIASES.get(n, n)


def blob_sha(b: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(b) + b).hexdigest()


def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def dump(obj) -> str:
    return json.dumps(obj, indent=2, ensure_ascii=False) + "\n"


def keep_set_count(entry: dict) -> dict:
    """Return entry with only set_count functions (key dropped if none left and none before)."""
    out = dict(entry)
    if "functions" in entry:
        sc = [f for f in entry["functions"] if fn_name(f) == "set_count"]
        if sc:
            out["functions"] = copy.deepcopy(sc)
        else:
            del out["functions"]
    return out


class FileJob:
    """One raw file: builds the normalised copy, the override, and the report rows."""

    def __init__(self, rel: str, domain: str, mapping: dict[str, str]):
        self.rel = rel
        self.domain = domain
        self.m = mapping
        self.rows: list[dict] = []
        self.missing: list[str] = []
        self.aliased: list[tuple[str, str]] = []

    def item(self, src: str) -> tuple[str, str]:
        """-> (normalised source id, mapped id). Records problems."""
        c = canon(src)
        if c != norm(src):
            tag = " [representative]" if norm(src) in REPRESENTATIVE else ""
            self.aliased.append((src + tag, c))
        if c not in self.m:
            self.missing.append(src)
            return c, c
        self.rows.append({"file": self.rel, "from": c, "to": self.m[c]})
        return c, self.m[c]

    # --- recipes -----------------------------------------------------------
    def recipe_entry(self, e):
        if isinstance(e, str):
            c, t = self.item(e)
            return c, t
        n = dict(e)
        o = dict(e)
        key = (norm(e["item"]), e.get("data"))
        if key in DATA_ALIASES:
            self.aliased.append((f"{key[0]} data {key[1]}", DATA_ALIASES[key]))
            n["item"] = DATA_ALIASES[key]
            n.pop("data", None)
            e = n
        c, t = self.item(e["item"])
        n["item"] = c
        o["item"] = t
        o.pop("data", None)
        return n, o

    def recipe(self, raw):
        normd = copy.deepcopy(raw)
        over = copy.deepcopy(raw)
        for rtype, body in raw.items():
            if not isinstance(body, dict):
                continue
            for key in ("result", "output"):
                if key not in body:
                    continue
                v = body[key]
                if isinstance(v, list):
                    pairs = [self.recipe_entry(x) for x in v]
                    normd[rtype][key] = [p[0] for p in pairs]
                    over[rtype][key] = [p[1] for p in pairs]
                else:
                    a, b = self.recipe_entry(v)
                    normd[rtype][key] = a
                    over[rtype][key] = b
        return normd, over

    # --- loot --------------------------------------------------------------
    def loot(self, node):
        if isinstance(node, dict):
            if node.get("type") == "item" and isinstance(node.get("name"), str):
                src = node["name"]
                data = [f.get("data") for f in node.get("functions", []) if fn_name(f) == "set_data"]
                key = (norm(src), data[0] if data and isinstance(data[0], int) else None)
                if key in DATA_ALIASES:
                    self.aliased.append((f"{key[0]} set_data {key[1]}", DATA_ALIASES[key]))
                    src = DATA_ALIASES[key]
                c, t = self.item(src)
                n = dict(node)
                n["name"] = c
                o = keep_set_count(node)
                o["name"] = t
                return n, o
            n, o = {}, {}
            for k, v in node.items():
                n[k], o[k] = self.loot(v)
            return n, o
        if isinstance(node, list):
            pairs = [self.loot(x) for x in node]
            return [p[0] for p in pairs], [p[1] for p in pairs]
        return node, node

    # --- trades ------------------------------------------------------------
    def gives(self, node):
        if isinstance(node, list):
            pairs = [self.gives(x) for x in node]
            return [p[0] for p in pairs], [p[1] for p in pairs]
        if isinstance(node, dict) and "choice" in node:
            n, o = dict(node), dict(node)
            n["choice"], o["choice"] = self.gives(node["choice"])
            return n, o
        if isinstance(node, dict) and "item" in node:
            c, t = self.item(node["item"])
            n = dict(node)
            n["item"] = c
            o = keep_set_count(node)
            o["item"] = t
            return n, o
        self.missing.append(f"<unexpected gives shape {node!r}>")
        return node, node

    def trades(self, node):
        if isinstance(node, dict):
            n, o = {}, {}
            for k, v in node.items():
                if k == "gives":
                    n[k], o[k] = self.gives(v)
                else:
                    n[k], o[k] = self.trades(v)
            return n, o
        if isinstance(node, list):
            pairs = [self.trades(x) for x in node]
            return [p[0] for p in pairs], [p[1] for p in pairs]
        return node, node


def classify(rel: str, raw) -> str | None:
    if rel.startswith("recipes/"):
        if "minecraft:recipe_furnace" in raw:
            return "smelting"
        if "minecraft:recipe_shaped" in raw or "minecraft:recipe_shapeless" in raw:
            return "crafting"
        return None
    if rel.startswith("loot_tables/chests/"):
        return "chest_loot"
    if rel.startswith("trading/"):
        return "trades"
    return None  # entities / gameplay (fishing) are runtime domains: never overridden here


def check_sources() -> dict:
    """Verify every raw file against SOURCES.json (git blob sha from the upstream repo)."""
    src_path = RAW / "SOURCES.json"
    sources = load(src_path)
    files = sources["files"]
    bad = []
    for rel, meta in files.items():
        p = RAW / rel
        if not p.exists():
            bad.append(f"{rel}: listed in SOURCES.json but missing")
            continue
        b = p.read_bytes()
        if len(b) != meta["size"] or blob_sha(b) != meta["sha"]:
            bad.append(f"{rel}: content does not match upstream blob {meta['sha']}")
    for sub in SUBDIRS:
        for p in sorted((RAW / sub).rglob("*.json")) if (RAW / sub).exists() else []:
            rel = p.relative_to(RAW).as_posix()
            if rel not in files:
                print(f"unverified raw file ignored (no sha in SOURCES.json): {rel}")
    if bad:
        raise SystemExit("raw vanilla copies failed verification:\n  " + "\n  ".join(bad))
    return sources


def main() -> int:
    sources = check_sources()
    universe = load(DATA / "item_universe.json")
    build_seed = load(DATA / "build_config.json")["build_seed"]
    seed = remap_engine.hash_string(build_seed)
    mapping = remap_engine.build_permutation(seed, universe)

    report = {"build_seed": build_seed, "seed": seed, "domains": {d: [] for d in DOMAIN_ORDER}}
    written: set[str] = set()
    skipped: list[str] = []
    aliases_used: dict[str, str] = {}
    counts = {d: [0, 0] for d in DOMAIN_ORDER}  # files, items

    for rel in sorted(sources["files"]):
        if not rel.split("/")[0] in SUBDIRS:
            continue
        raw = load(RAW / rel)
        domain = classify(rel, raw)
        if domain is None:
            skipped.append(f"{rel}: not a handled data domain")
            continue
        job = FileJob(rel, domain, mapping)
        if domain in ("crafting", "smelting"):
            normd, over = job.recipe(raw)
            # Recipe-type key first, then "format_version" (JSON key order has no meaning to
            # the game; tools and tests read the first value as the recipe body).
            over = {k: over[k] for k in sorted(over, key=lambda k: not k.startswith("minecraft:recipe_"))}
        elif domain == "chest_loot":
            normd, over = job.loot(raw)
        else:
            normd, over = job.trades(raw)
        if job.missing:
            skipped.append(f"{rel}: ids not in universe: {sorted(set(job.missing))}")
            continue
        if not job.rows:
            skipped.append(f"{rel}: no remap-zone items")
            continue
        for a, c in job.aliased:
            aliases_used[a] = c
        for path, obj in ((NORM / rel, normd), (PACK / rel, over)):
            path.parent.mkdir(parents=True, exist_ok=True)
            text = dump(obj)
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                path.write_text(text, encoding="utf-8")
        written.add(rel)
        report["domains"][domain].extend(job.rows)
        counts[domain][0] += 1
        counts[domain][1] += len(job.rows)

    # Delete stale generated files (pack overrides and normalised copies with no source).
    for base in (PACK, NORM):
        for sub in SUBDIRS:
            d = base / sub
            if not d.exists():
                continue
            for p in sorted(d.rglob("*.json")):
                rel = p.relative_to(base).as_posix()
                if rel not in written:
                    p.unlink()
                    print(f"removed stale {p.relative_to(ROOT)}")
            for sub_dir in sorted((x for x in d.rglob("*") if x.is_dir()), reverse=True):
                if not any(sub_dir.iterdir()):
                    sub_dir.rmdir()

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"source: {sources['repo']} @ {sources['ref']}")
    print(f"build_seed={build_seed!r} seed={seed}")
    for d in DOMAIN_ORDER:
        print(f"{d:<11} files={counts[d][0]:>3} remapped_items={counts[d][1]:>4}")
    if aliases_used:
        print("legacy aliases applied:")
        for a in sorted(aliases_used):
            print(f"  {a} -> {aliases_used[a]}")
    if skipped:
        print("skipped (keep vanilla behaviour):")
        for s in skipped:
            print("  " + s)
    print(f"wrote {REPORT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
