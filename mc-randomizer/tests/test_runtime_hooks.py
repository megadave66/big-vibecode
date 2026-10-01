"""Runtime hook tests: drive the real pack scripts under node with a mock @minecraft/server.

Each scenario copies behavior_pack/scripts to a temp dir, installs tests/mock/minecraft_server.js
as node_modules/@minecraft/server, runs a scenario script and parses its JSON output.
Run: cd R && python3 -m pytest tests/test_runtime_hooks.py -q
"""
import json
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "behavior_pack" / "scripts"
MOCK = ROOT / "tests" / "mock" / "minecraft_server.js"

PRELUDE = r"""
import { world, system, ItemStack, mock } from "@minecraft/server";
import { createRemapper } from "./scripts/remap_engine.js";
import { ITEM_UNIVERSE } from "./scripts/item_universe.js";

export const OW = "minecraft:overworld";
export const spawnLog = [];
const dim = world.getDimension(OW);
const origSpawn = dim.spawnItem.bind(dim);
dim.spawnItem = (stack, loc) => { const e = origSpawn(stack, loc); spawnLog.push({ typeId: stack.typeId, amount: stack.amount }); return e; };

export async function setup() {
  let mode = "main", err = null, remapper, seed;
  try {
    await import("./scripts/main.js");
    world.afterEvents.worldInitialize.fire({});
    seed = world.getDynamicProperty("mcr:seed");
    remapper = createRemapper(seed >>> 0, ITEM_UNIVERSE);
  } catch (e) {
    err = String(e && e.stack || e);
    mode = "direct";
  }
  if (mode === "direct") {
    // main.js missing or broken: wire the S3 pieces by hand.
    const { createInterceptor } = await import("./scripts/drop_interceptor.js");
    const { install } = await import("./scripts/hooks/blocks_mobs.js");
    seed = 12345;
    remapper = createRemapper(seed, ITEM_UNIVERSE);
    const interceptor = createInterceptor(remapper);
    install({ seed, remapper, interceptor, log: (m) => console.warn(m) });
  }
  return { mode, mainError: err, remapper, seed, mock, world, system, ItemStack, OW, spawnLog, ITEM_UNIVERSE };
}
export const inU = (r, id) => r.map(id) !== undefined;
export const maxOf = (id) => new ItemStack(id, 1).maxAmount;
export const out = (o) => console.log("@@JSON@@" + JSON.stringify(o));
"""


def run_scenario(body, mode_required=None):
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="mcr_rt_"))
    try:
        shutil.copytree(SCRIPTS, tmp / "scripts")
        (tmp / "package.json").write_text('{"type":"module"}')
        mod = tmp / "node_modules" / "@minecraft" / "server"
        mod.mkdir(parents=True)
        shutil.copy(MOCK, mod / "index.js")
        (mod / "package.json").write_text('{"name":"@minecraft/server","type":"module","main":"index.js"}')
        (tmp / "prelude.mjs").write_text(PRELUDE)
        (tmp / "scenario.mjs").write_text(
            'import * as P from "./prelude.mjs";\nconst { OW, out, inU, maxOf } = P;\n'
            "const T = await P.setup();\nconst { mock, remapper, world } = T;\n"
            "const result = await (async () => {\n" + body + "\n})();\n"
            "out({ mode: T.mode, mainError: T.mainError, ...result });\n"
        )
        p = subprocess.run(["node", "scenario.mjs"], cwd=tmp, capture_output=True, text=True, timeout=60)
        lines = [l for l in p.stdout.splitlines() if l.startswith("@@JSON@@")]
        if p.returncode != 0 or not lines:
            raise AssertionError(f"node failed rc={p.returncode}\nSTDOUT:{p.stdout}\nSTDERR:{p.stderr}")
        return json.loads(lines[-1][len("@@JSON@@"):])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


class TestRuntimeHooks(unittest.TestCase):
    def test_00_main_wiring(self):
        """main.js loads and installs the hooks on worldInitialize (wiring test)."""
        r = run_scenario("return {};")
        self.assertEqual(r["mode"], "main", r["mainError"])

    def test_01_dirt_break_remapped(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
const b = mock.createBlock(OW, "minecraft:dirt", {x:10,y:64,z:10});
const [orig] = mock.breakBlock(p, b, [{typeId:"minecraft:dirt", amount:3}]);
mock.tick(6);
return { expected: remapper.map("minecraft:dirt"), items: mock.liveItems(), origValid: orig.isValid() };
""")
        self.assertIsNotNone(r["expected"])
        self.assertNotEqual(r["expected"], "minecraft:dirt")
        self.assertFalse(r["origValid"])
        self.assertEqual(sum(i["amount"] for i in r["items"]), 3)
        self.assertEqual({i["typeId"] for i in r["items"]}, {r["expected"]})

    def test_02_same_block_same_item(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
mock.breakBlock(p, mock.createBlock(OW, "minecraft:dirt", {x:10,y:64,z:10}), [{typeId:"minecraft:dirt", amount:1}]);
mock.tick(6);
const first = mock.liveItems().map(i => i.typeId);
mock.breakBlock(p, mock.createBlock(OW, "minecraft:dirt", {x:30,y:64,z:-5}), [{typeId:"minecraft:dirt", amount:1}]);
mock.tick(6);
return { first, all: mock.liveItems().map(i => i.typeId), expected: remapper.map("minecraft:dirt") };
""")
        self.assertEqual(r["first"], [r["expected"]])
        self.assertEqual(r["all"], [r["expected"], r["expected"]])

    def test_03_split_65_across_max_stack(self):
        r = run_scenario(r"""
// find one universe source for each target stack size 1 / 16 / 64
const found = {};
for (const s of T.ITEM_UNIVERSE) {
  const t = remapper.map(s);
  if (t === undefined) continue;
  const m = maxOf(t);
  if (!(m in found)) found[m] = [s, t];
}
const p = mock.createPlayer();
const res = {};
let x = 0;
for (const m of Object.keys(found)) {
  const [s, t] = found[m];
  x += 20;
  mock.breakBlock(p, mock.createBlock(OW, "minecraft:stone", {x,y:64,z:0}), [{typeId:s, amount:65}]);
  mock.tick(6);
  const mine = mock.liveItems().filter(i => i.typeId === t);
  res[m] = { src: s, target: t, stacks: mine.map(i => i.amount), total: mine.reduce((a,i)=>a+i.amount,0) };
  for (const e of world.getDimension(OW).getEntities()) if (e.typeId === "minecraft:item") e.remove();
}
return { res };
""")
        res = r["res"]
        for need in ("1", "16", "64"):
            self.assertIn(need, res, f"universe has no target with maxAmount {need}")
        for m, d in res.items():
            self.assertEqual(d["total"], 65, d)
            self.assertTrue(all(a <= int(m) for a in d["stacks"]), d)
        self.assertEqual(len(res["1"]["stacks"]), 65)

    def test_04_chest_contents_not_remapped(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
const chest = mock.createBlock(OW, "minecraft:chest", {x:5,y:64,z:5}, [{typeId:"minecraft:stick", amount:1}]);
const [chestItem, stick] = mock.breakBlock(p, chest, [{typeId:"minecraft:chest", amount:1}, {typeId:"minecraft:stick", amount:1}]);
mock.tick(6);
return { chestValid: chestItem.isValid(), stickValid: stick.isValid(),
  chestInU: inU(remapper, "minecraft:chest"), items: mock.liveItems(), expected: remapper.map("minecraft:chest") };
""")
        self.assertTrue(r["chestInU"])
        self.assertFalse(r["chestValid"])
        self.assertTrue(r["stickValid"])
        types = sorted(i["typeId"] for i in r["items"])
        self.assertEqual(types, sorted(["minecraft:stick", r["expected"]]))

    def test_05_sheep_drops_remapped(self):
        r = run_scenario(r"""
const sheep = mock.createEntity(OW, "minecraft:sheep", {x:8,y:64,z:8});
mock.killEntity(sheep, [{typeId:"minecraft:white_wool", amount:1}, {typeId:"minecraft:mutton", amount:2}]);
mock.tick(6);
return { items: mock.liveItems(), wool: remapper.map("minecraft:white_wool"), mutton: remapper.map("minecraft:mutton") };
""")
        got = sorted((i["typeId"], i["amount"]) for i in r["items"])
        self.assertEqual(got, sorted([(r["wool"], 1), (r["mutton"], 2)]))

    def test_06_ender_dragon_untouched(self):
        r = run_scenario(r"""
const d = mock.createEntity(OW, "minecraft:ender_dragon", {x:0,y:70,z:0});
const drops = mock.killEntity(d, [{typeId:"minecraft:dirt", amount:2}]);
mock.tick(10);
return { valid: drops[0].isValid(), items: mock.liveItems() };
""")
        self.assertTrue(r["valid"])
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:dirt", 2)])

    def test_07_player_death_untouched(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
const drops = mock.killEntity(p, [{typeId:"minecraft:dirt", amount:5}]);
mock.tick(10);
return { valid: drops[0].isValid(), items: mock.liveItems() };
""")
        self.assertTrue(r["valid"])
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:dirt", 5)])

    def test_08_far_or_no_source_untouched(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
// no source at all: a player toss
const toss = mock.spawnItemEntity(OW, "minecraft:dirt", 1, {x:0,y:64,z:0}, "Spawned");
// a source exists, but the item is far away
mock.breakBlock(p, mock.createBlock(OW, "minecraft:stone", {x:100,y:64,z:100}), []);
const far = mock.spawnItemEntity(OW, "minecraft:dirt", 1, {x:140,y:64,z:100}, "Spawned");
// a source exists in another dimension
mock.breakBlock(p, mock.createBlock("minecraft:nether", "minecraft:stone", {x:-50,y:64,z:-50}), []);
const otherDim = mock.spawnItemEntity(OW, "minecraft:dirt", 1, {x:-50,y:64,z:-50}, "Spawned");
mock.tick(60);
return { valid: [toss.isValid(), far.isValid(), otherDim.isValid()] };
""")
        self.assertEqual(r["valid"], [True, True, True])

    def test_09_no_double_remap(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
mock.breakBlock(p, mock.createBlock(OW, "minecraft:dirt", {x:10,y:64,z:10}), [{typeId:"minecraft:dirt", amount:1}]);
mock.tick(60);
const m1 = remapper.map("minecraft:dirt");
return { items: mock.liveItems(), spawnLog: P.spawnLog, m1, m2: remapper.map(m1) };
""")
        self.assertEqual([i["typeId"] for i in r["items"]], [r["m1"]])
        self.assertEqual(len(r["spawnLog"]), 1)  # exactly one remap spawn
        self.assertNotEqual(r["m2"], r["m1"])

    def test_10_outside_universe_untouched(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
const [e] = mock.breakBlock(p, mock.createBlock(OW, "minecraft:stone", {x:1,y:64,z:1}), [{typeId:"minecraft:command_block", amount:1}]);
mock.tick(10);
return { valid: e.isValid(), mapped: remapper.map("minecraft:command_block") ?? null, items: mock.liveItems() };
""")
        self.assertIsNone(r["mapped"])
        self.assertTrue(r["valid"])
        self.assertEqual([i["typeId"] for i in r["items"]], ["minecraft:command_block"])

    def test_11_loaded_cause_untouched(self):
        r = run_scenario(r"""
const p = mock.createPlayer();
mock.breakBlock(p, mock.createBlock(OW, "minecraft:stone", {x:1,y:64,z:1}), []);
const e = mock.spawnItemEntity(OW, "minecraft:dirt", 1, {x:1.5,y:64.5,z:1.5}, "Loaded");
mock.tick(10);
return { valid: e.isValid() };
""")
        self.assertTrue(r["valid"])

    def test_12_source_recorded_after_item_spawn_same_tick(self):
        """Mob death event may come after the drop spawns; the item waits for its source."""
        r = run_scenario(r"""
const sheep = mock.createEntity(OW, "minecraft:sheep", {x:8,y:64,z:8});
const e = mock.spawnItemEntity(OW, "minecraft:mutton", 1, sheep.location, "Spawned");
mock.tick(1);                       // item spawn event delivered, source not yet recorded
world.afterEvents.entityDie.fire({ deadEntity: sheep, damageSource: {} });
mock.tick(5);
return { valid: e.isValid(), items: mock.liveItems(), expected: remapper.map("minecraft:mutton") };
""")
        self.assertFalse(r["valid"])
        self.assertEqual([i["typeId"] for i in r["items"]], [r["expected"]])

    def test_13_source_text_contract(self):
        txt = (SCRIPTS / "hooks" / "blocks_mobs.js").read_text()
        self.assertIn("playerBreakBlock", txt)
        self.assertIn("EXCLUDED_ENTITIES", txt)


if __name__ == "__main__":
    unittest.main()
