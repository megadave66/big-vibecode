"""Static test suite for the mc-randomizer behavior pack.

Written BEFORE implementation (it is the spec). Run from the project root:

    python3 -m unittest tests/test_remaps.py -v      (or: python3 -m pytest tests -q)

No Minecraft install is needed. In-game checks live in TESTS.md.
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import uuid
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACK = ROOT / "behavior_pack"
SCRIPTS = PACK / "scripts"
DATA = ROOT / "data"
VANILLA = DATA / "vanilla"
TOOLS = ROOT / "tools"
DIST = ROOT / "dist" / "mc-randomizer.mcaddon"
DEV = ROOT / "dev"

sys.path.insert(0, str(TOOLS))

EXPECTED_DOMAINS = {
    "block_drops",
    "mob_drops",
    "crafting",
    "smelting",
    "chest_loot",
    "fishing",
    "trades",
}
DRAGON = "minecraft:ender_dragon"
ID_RE = re.compile(r"^minecraft:[a-z0-9_.]+$")
SEEDS = [0, 1, 2, 7, 42, 12345, 99999, 2**31 - 1, 2**31, 2**32 - 1, 3735928559]


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def universe() -> list[str]:
    return load_json(DATA / "item_universe.json")


def engine():
    import remap_engine  # tools/remap_engine.py

    return remap_engine


def build_seed() -> int:
    cfg = load_json(DATA / "build_config.json")
    return engine().hash_string(cfg["build_seed"])


def run_node(js: str) -> str:
    """Run an ES module snippet against a temp copy of behavior_pack/scripts."""
    with tempfile.TemporaryDirectory() as tmp:
        dst = Path(tmp) / "scripts"
        shutil.copytree(SCRIPTS, dst)
        (dst / "package.json").write_text('{"type":"module"}')
        runner = dst / "_runner.js"
        runner.write_text(js)
        out = subprocess.run(
            ["node", str(runner)], capture_output=True, text=True, timeout=120
        )
        if out.returncode != 0:
            raise AssertionError(f"node failed:\n{out.stderr}")
        return out.stdout


# ---------------------------------------------------------------------------
# A. Remap engine (Python port in tools/remap_engine.py)
# ---------------------------------------------------------------------------
class TestEnginePython(unittest.TestCase):
    def test_hash_string_known_values(self):
        e = engine()
        # FNV-1a 32-bit reference values.
        self.assertEqual(e.hash_string(""), 0x811C9DC5)
        self.assertEqual(e.hash_string("a"), 0xE40C292C)
        self.assertEqual(e.hash_string("foobar"), 0xBF9CF968)

    def test_mulberry32_range_and_determinism(self):
        e = engine()
        a = e.mulberry32(123)
        b = e.mulberry32(123)
        xs = [a() for _ in range(1000)]
        ys = [b() for _ in range(1000)]
        self.assertEqual(xs, ys)
        self.assertTrue(all(0.0 <= x < 1.0 for x in xs))

    def test_deterministic_for_fixed_seed(self):
        e = engine()
        u = universe()
        for s in SEEDS:
            self.assertEqual(e.build_permutation(s, u), e.build_permutation(s, u))

    def test_different_seeds_differ(self):
        e = engine()
        u = universe()
        maps = [tuple(sorted(e.build_permutation(s, u).items())) for s in SEEDS]
        self.assertEqual(len(set(maps)), len(maps))

    def test_bijection_and_zero_identity(self):
        e = engine()
        u = universe()
        total_identity = 0
        for s in SEEDS + [build_seed()]:
            m = e.build_permutation(s, u)
            self.assertEqual(set(m.keys()), set(u))
            self.assertEqual(sorted(m.values()), sorted(u), "must be a bijection")
            total_identity += sum(1 for k, v in m.items() if k == v)
        self.assertEqual(total_identity, 0, "identity remaps found")

    def test_universe_order_independent(self):
        e = engine()
        u = universe()
        self.assertEqual(
            e.build_permutation(5, u), e.build_permutation(5, list(reversed(u)))
        )

    def test_split_count_preserves_quantity(self):
        e = engine()
        self.assertEqual(e.split_count(0, 64), [])
        self.assertEqual(e.split_count(1, 64), [1])
        self.assertEqual(e.split_count(64, 64), [64])
        self.assertEqual(e.split_count(65, 64), [64, 1])
        self.assertEqual(e.split_count(5, 1), [1, 1, 1, 1, 1])
        self.assertEqual(e.split_count(20, 16), [16, 4])
        # Bad inputs must not hang or raise.
        # A bad max stack falls back to stacks of 1, so the count is kept.
        for mx in (0, -1, float("inf"), float("nan")):
            self.assertEqual(e.split_count(5, mx), [1, 1, 1, 1, 1], mx)
        for total in (float("inf"), float("nan"), -3):
            self.assertEqual(e.split_count(total, 64), [], total)
        for total in range(0, 200):
            for mx in (1, 16, 64):
                parts = e.split_count(total, mx)
                self.assertEqual(sum(parts), total)
                self.assertTrue(all(1 <= p <= mx for p in parts))


# ---------------------------------------------------------------------------
# B. JS engine == Python port (the pack runs the JS one)
# ---------------------------------------------------------------------------
class TestEngineParity(unittest.TestCase):
    def test_js_matches_python(self):
        e = engine()
        u = universe()
        seeds = SEEDS + [build_seed()]
        js = f"""
import * as eng from './remap_engine.js';
import * as prng from './prng.js';
import {{ ITEM_UNIVERSE }} from './item_universe.js';
const seeds = {json.dumps(seeds)};
const out = {{
  hashes: ["", "a", "foobar", "mc-randomizer-build-v1"].map(prng.hashString),
  rng: (() => {{ const r = prng.mulberry32(99); return Array.from({{length: 20}}, () => r()); }})(),
  universe: ITEM_UNIVERSE,
  maps: seeds.map(s => eng.buildPermutation(s, ITEM_UNIVERSE)),
  split: [[0,64],[65,64],[5,1],[20,16],[130,64]].map(([t,m]) => eng.splitCount(t,m)),
  domains: eng.DOMAINS,
  excluded: eng.EXCLUDED_ENTITIES,
}};
console.log(JSON.stringify(out));
"""
        res = json.loads(run_node(js))
        self.assertEqual(
            res["hashes"],
            [e.hash_string(x) for x in ["", "a", "foobar", "mc-randomizer-build-v1"]],
        )
        r = e.mulberry32(99)
        self.assertEqual(res["rng"], [r() for _ in range(20)])
        self.assertEqual(res["universe"], u, "item_universe.js must equal data/item_universe.json")
        for s, jm in zip(seeds, res["maps"]):
            self.assertEqual(jm, e.build_permutation(s, u), f"seed {s} differs")
        self.assertEqual(
            res["split"],
            [e.split_count(t, m) for t, m in [(0, 64), (65, 64), (5, 1), (20, 16), (130, 64)]],
        )
        self.assertEqual(set(res["domains"]), EXPECTED_DOMAINS)
        self.assertEqual(len(res["domains"]), 7)
        self.assertIn(DRAGON, res["excluded"])

    def test_engine_is_pure(self):
        src = (SCRIPTS / "remap_engine.js").read_text()
        self.assertNotIn("@minecraft/server", src, "engine must stay pure (node-testable)")


# ---------------------------------------------------------------------------
# C. Item universe
# ---------------------------------------------------------------------------
class TestItemUniverse(unittest.TestCase):
    def test_sorted_unique_valid(self):
        u = universe()
        self.assertEqual(u, sorted(set(u)))
        self.assertGreaterEqual(len(u), 800)
        for i in u:
            self.assertRegex(i, ID_RE)

    def test_subset_of_vanilla_snapshot(self):
        snap = set(load_json(DATA / "vanilla_items_1.21.0.json"))
        missing = [i for i in universe() if i not in snap]
        self.assertEqual(missing, [], "ids not in @minecraft/vanilla-data 1.21.0")

    def test_exclusions_applied(self):
        u = set(universe())
        excl = load_json(DATA / "item_exclusions.json")
        self.assertIsInstance(excl, dict)
        self.assertEqual(sorted(u & set(excl)), [])
        for bad in [
            "minecraft:command_block", "minecraft:barrier", "minecraft:bedrock",
            "minecraft:structure_block", "minecraft:structure_void", "minecraft:jigsaw",
            "minecraft:debug_stick", "minecraft:light_block", "minecraft:allow",
            "minecraft:deny", "minecraft:border_block", "minecraft:air",
        ]:
            self.assertNotIn(bad, u)
        self.assertEqual([i for i in u if i.endswith("_spawn_egg")], [])

    def test_common_items_present(self):
        u = set(universe())
        for good in [
            "minecraft:dirt", "minecraft:diamond", "minecraft:furnace",
            "minecraft:cobblestone", "minecraft:white_wool", "minecraft:mutton",
            "minecraft:cod", "minecraft:emerald", "minecraft:stick",
            "minecraft:iron_ingot", "minecraft:rotten_flesh", "minecraft:bone",
        ]:
            self.assertIn(good, u)


# ---------------------------------------------------------------------------
# D. Every domain has a real mechanism
# ---------------------------------------------------------------------------
def all_script_text() -> str:
    return "\n".join(p.read_text() for p in SCRIPTS.rglob("*.js"))


def pack_json(sub: str) -> list[Path]:
    base = PACK / sub
    return sorted(base.rglob("*.json")) if base.exists() else []


class TestDomainCoverage(unittest.TestCase):
    def test_runtime_domains_hooked(self):
        txt = all_script_text()
        self.assertIn("playerBreakBlock", txt)       # block_drops
        self.assertIn("entityDie", txt)              # mob_drops
        self.assertIn("entitySpawn", txt)            # item-entity interception
        self.assertIn("minecraft:fishing_hook", txt)  # fishing
        self.assertIn("getWorldSeed", txt)           # world-seeded

    def test_crafting_overrides_exist(self):
        files = pack_json("recipes")
        shaped = [f for f in files if any(k in load_json(f) for k in ("minecraft:recipe_shaped", "minecraft:recipe_shapeless"))]
        self.assertGreaterEqual(len(shaped), 20)
        ids = {next(iter(load_json(f).values()))["description"]["identifier"] for f in shaped}
        self.assertIn("minecraft:furnace", ids, "furnace crafting recipe must be randomized")

    def test_smelting_overrides_exist(self):
        files = [f for f in pack_json("recipes") if "minecraft:recipe_furnace" in load_json(f)]
        self.assertGreaterEqual(len(files), 10)

    def test_chest_loot_overrides_exist(self):
        self.assertTrue((PACK / "loot_tables/chests/simple_dungeon.json").exists())
        self.assertGreaterEqual(len(pack_json("loot_tables/chests")), 5)

    def test_trade_overrides_exist(self):
        self.assertGreaterEqual(len(pack_json("trading")), 5)

    def test_no_double_handling(self):
        # Mob drops and fishing are runtime-remapped; overriding their loot
        # tables too would remap twice (and could create identity results).
        self.assertFalse((PACK / "loot_tables/entities").exists())
        self.assertEqual(list((PACK / "loot_tables").glob("gameplay/fishing*")), [])


# ---------------------------------------------------------------------------
# E. Data-driven overrides really change every output item, keep counts
# ---------------------------------------------------------------------------
def norm(i: str) -> str:
    return i if ":" in i else "minecraft:" + i


class OverrideChecker:
    def __init__(self, mapping: dict[str, str]):
        self.m = mapping
        self.remapped = 0
        self.counts_checked = 0
        self.errors: list[str] = []

    def err(self, path, msg):
        self.errors.append(f"{path}: {msg}")

    def same(self, a, b, path):
        if type(a) is not type(b):
            self.err(path, f"type differs {a!r} vs {b!r}")
        elif isinstance(a, dict):
            if set(a) != set(b):
                self.err(path, f"keys differ {sorted(a)} vs {sorted(b)}")
            for k in a:
                if k in b:
                    self.walk(a[k], b[k], f"{path}/{k}", k)
        elif isinstance(a, list):
            if len(a) != len(b):
                self.err(path, "list length differs")
            for i, (x, y) in enumerate(zip(a, b)):
                self.walk(x, y, f"{path}[{i}]", None)
        elif a != b:
            self.err(path, f"{a!r} != {b!r}")

    def check_item(self, src_id, out_id, path):
        s = norm(src_id)
        if s not in self.m:
            self.err(path, f"source item {s} not in item universe")
            return
        if norm(out_id) == s:
            self.err(path, f"identity remap {s}")
        elif norm(out_id) != self.m[s]:
            self.err(path, f"{s} -> {out_id}, expected {self.m[s]}")
        else:
            self.remapped += 1

    def zone_entry(self, a, b, path, key):
        """An item entry: id remapped, quantity preserved, other keys free."""
        if isinstance(a, str):
            # A bare id means a count of 1; the output must stay a bare id.
            if not isinstance(b, str):
                self.err(path, "bare id output became an object")
                return
            self.check_item(a, b, path)
            self.counts_checked += 1
            return
        if not isinstance(b, dict) or key not in a or key not in b:
            self.err(path, "bad item entry")
            return
        self.check_item(a[key], b[key], path)
        for qk in ("count", "quantity"):
            if qk in a and a.get(qk) != b.get(qk):
                self.err(path, f"{qk} changed")
        # Bedrock accepts "set_count" and "minecraft:set_count"; both carry the count.
        sc = lambda e: [
            f for f in e.get("functions", [])
            if str(f.get("function", "")).removeprefix("minecraft:") == "set_count"
        ]
        if sc(a) != sc(b):
            self.err(path, "set_count changed")
        self.counts_checked += 1

    def walk(self, a, b, path, key):
        raise NotImplementedError


class RecipeChecker(OverrideChecker):
    def walk(self, a, b, path, key):
        if key in ("result", "output"):
            items = a if isinstance(a, list) else [a]
            outs = b if isinstance(b, list) else [b]
            if len(items) != len(outs):
                self.err(path, "result count differs")
            for i, (x, y) in enumerate(zip(items, outs)):
                self.zone_entry(x, y, f"{path}[{i}]", "item")
        else:
            self.same(a, b, path)


class LootChecker(OverrideChecker):
    def walk(self, a, b, path, key):
        if isinstance(a, dict) and a.get("type") == "item":
            self.zone_entry(a, b, path, "name")
            rest = lambda e: {k: v for k, v in e.items() if k not in ("name", "functions")}
            if isinstance(b, dict) and rest(a) != rest(b):
                self.err(path, "non-item fields changed")
        else:
            self.same(a, b, path)


class TradeChecker(OverrideChecker):
    def walk(self, a, b, path, key):
        if key == "gives":
            self.gives(a, b, path)
        else:
            self.same(a, b, path)

    def gives(self, a, b, path):
        if isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
            for i, (x, y) in enumerate(zip(a, b)):
                self.gives(x, y, f"{path}[{i}]")
        elif isinstance(a, dict) and "choice" in a and isinstance(b, dict):
            self.gives(a["choice"], b.get("choice"), f"{path}/choice")
        elif isinstance(a, dict) and "item" in a:
            self.zone_entry(a, b, path, "item")
        else:
            self.err(path, "unexpected gives shape")


class TestDataOverrides(unittest.TestCase):
    def setUp(self):
        self.mapping = engine().build_permutation(build_seed(), universe())

    def run_checker(self, sub, cls, min_remapped):
        files = pack_json(sub)
        self.assertTrue(files, f"no files in behavior_pack/{sub}")
        chk = cls(self.mapping)
        for f in files:
            rel = f.relative_to(PACK)
            src = VANILLA / rel
            if not src.exists():
                chk.err(str(rel), "no vanilla source copy in data/vanilla")
                continue
            chk.walk(load_json(src), load_json(f), str(rel), None)
        self.assertEqual(chk.errors[:25], [], f"{len(chk.errors)} errors")
        self.assertGreaterEqual(chk.remapped, min_remapped)
        # Every remapped entry had its quantity compared one by one.
        self.assertGreaterEqual(chk.counts_checked, chk.remapped)
        return chk

    def test_set_count_checker_catches_namespaced_drop(self):
        # Guard for a real bug: "minecraft:set_count" was once dropped unnoticed.
        u = universe()
        src = {"type": "item", "name": u[0],
               "functions": [{"function": "minecraft:set_count", "count": {"min": 1, "max": 3}}]}
        out = {"type": "item", "name": self.mapping[u[0]], "functions": []}
        chk = LootChecker(self.mapping)
        chk.walk(src, out, "x", None)
        self.assertTrue(any("set_count" in e for e in chk.errors))

    def test_recipes(self):
        self.run_checker("recipes", RecipeChecker, 30)

    def test_recipe_identifiers_keep_vanilla_names(self):
        for f in pack_json("recipes"):
            body = next(iter(load_json(f).values()))
            self.assertTrue(body["description"]["identifier"].startswith("minecraft:"), f)

    def test_loot_tables(self):
        self.run_checker("loot_tables", LootChecker, 30)

    def test_trades(self):
        self.run_checker("trading", TradeChecker, 30)


# ---------------------------------------------------------------------------
# F. Ender dragon untouched
# ---------------------------------------------------------------------------
class TestDragonExcluded(unittest.TestCase):
    def test_dragon_guard_in_runtime_hooks(self):
        txt = all_script_text()
        self.assertIn("EXCLUDED_ENTITIES", txt)
        self.assertIn(DRAGON, (SCRIPTS / "remap_engine.js").read_text())

    def test_no_dragon_data_overrides(self):
        for f in PACK.rglob("*.json"):
            self.assertNotIn("ender_dragon", f.name, f)


# ---------------------------------------------------------------------------
# G. Built .mcaddon: unpack and validate
# ---------------------------------------------------------------------------
class TestPackage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = subprocess.run(
            [sys.executable, str(TOOLS / "build.py")],
            cwd=ROOT, capture_output=True, text=True, timeout=600,
        )
        cls.build_out = out
        cls.tmp = tempfile.TemporaryDirectory()
        if out.returncode == 0 and DIST.exists():
            with zipfile.ZipFile(DIST) as z:
                z.extractall(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_build_succeeds(self):
        self.assertEqual(self.build_out.returncode, 0, self.build_out.stdout + self.build_out.stderr)
        self.assertTrue(DIST.exists())

    def manifests(self):
        return list(Path(self.tmp.name).glob("*/manifest.json"))

    def test_manifest(self):
        ms = self.manifests()
        self.assertEqual(len(ms), 1, "exactly one behavior pack folder at zip root")
        m = load_json(ms[0])
        self.assertEqual(m["format_version"], 2)
        h = m["header"]
        self.assertEqual(h["min_engine_version"][:2], [1, 21])
        uuids = [h["uuid"]] + [mod["uuid"] for mod in m["modules"]]
        for u in uuids:
            self.assertEqual(uuid.UUID(u).version, 4)
        self.assertEqual(len(set(uuids)), len(uuids))
        types = {mod["type"] for mod in m["modules"]}
        self.assertEqual(types, {"data", "script"})
        script = next(mod for mod in m["modules"] if mod["type"] == "script")
        self.assertEqual(script["language"], "javascript")
        self.assertTrue((ms[0].parent / script["entry"]).exists())
        deps = {d.get("module_name"): d.get("version") for d in m.get("dependencies", [])}
        self.assertEqual(deps.get("@minecraft/server"), "1.11.0")

    def test_all_json_parses_and_no_dev_files(self):
        root = Path(self.tmp.name)
        for f in root.rglob("*.json"):
            json.loads(f.read_text(encoding="utf-8"))
        names = [str(p.relative_to(root)) for p in root.rglob("*")]
        for bad in ("node_modules", "package.json", "tsconfig", ".py", "__pycache__", ".DS_Store"):
            self.assertFalse([n for n in names if bad in n], bad)

    def test_zip_matches_source_pack(self):
        root = self.manifests()[0].parent
        for f in PACK.rglob("*"):
            if f.is_file() and f.name != ".DS_Store":
                self.assertTrue((root / f.relative_to(PACK)).exists(), f)


# ---------------------------------------------------------------------------
# H. Scripts type-check against the real @minecraft/server 1.11.0 typings
# ---------------------------------------------------------------------------
class TestTypeCheck(unittest.TestCase):
    def test_tsc_check_js(self):
        tsc = DEV / "node_modules" / ".bin" / "tsc"
        self.assertTrue(tsc.exists(), "run: (cd dev && npm install)")
        typed = (DEV / "node_modules/@minecraft/server/package.json")
        self.assertEqual(load_json(typed)["version"], "1.11.0")
        out = subprocess.run(
            [str(tsc), "-p", str(DEV / "tsconfig.json")],
            capture_output=True, text=True, timeout=300,
        )
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
