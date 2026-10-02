"""Fishing hook tests: drive the real pack scripts under node with the mock @minecraft/server.

Reuses the harness in tests/test_runtime_hooks.py (run_scenario).
Run: cd R && python3 -m pytest tests/test_fishing_hook.py -q
"""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import test_runtime_hooks as rt  # noqa: E402  (module alias: pytest must not re-collect its tests)

HOOK = r"""
const p = mock.createPlayer(OW, {x:0, y:64, z:0});
const P = {x:20.3, y:62.1, z:-7.6};
"""


class TestFishingHook(unittest.TestCase):
    def test_catch_at_hook_is_remapped(self):
        """Hook removed at P, raw cod spawns at P next tick -> mapped item, same count."""
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity(OW, "minecraft:fishing_hook", P);
mock.tick(3);                       // live-hook scan sees it
hook.remove();                      // beforeEvents.entityRemove fires
mock.tick(1);
const orig = mock.spawnItemEntity(OW, "minecraft:cod", 1, {x:P.x+0.2, y:P.y+0.3, z:P.z});
mock.tick(8);
return { expected: remapper.map("minecraft:cod"), items: mock.liveItems(), origValid: orig.isValid() };
""")
        self.assertEqual(r["mode"], "main", r["mainError"])
        self.assertIsNotNone(r["expected"])
        self.assertNotEqual(r["expected"], "minecraft:cod")
        self.assertFalse(r["origValid"])
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [(r["expected"], 1)])

    def test_catch_same_tick_as_removal(self):
        """Catch item spawns before the hook is removed, in the same tick."""
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity(OW, "minecraft:fishing_hook", P);
mock.tick(2);
mock.spawnItemEntity(OW, "minecraft:salmon", 1, P);
hook.remove();
mock.tick(8);
return { expected: remapper.map("minecraft:salmon"), items: mock.liveItems() };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [(r["expected"], 1)])

    def test_fallback_when_no_remove_event(self):
        """Hook vanishes without entityRemove: the live-hook scan records the source."""
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity(OW, "minecraft:fishing_hook", P);
mock.tick(2);
hook._valid = false; hook.dimension._entities.delete(hook.id);   // silent removal
mock.spawnItemEntity(OW, "minecraft:cod", 1, P);
mock.tick(8);
return { expected: remapper.map("minecraft:cod"), items: mock.liveItems() };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [(r["expected"], 1)])

    def test_item_without_hook_untouched(self):
        r = rt.run_scenario(HOOK + r"""
mock.spawnItemEntity(OW, "minecraft:cod", 1, P);
mock.tick(10);
return { items: mock.liveItems() };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:cod", 1)])

    def test_hook_far_away_untouched(self):
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity(OW, "minecraft:fishing_hook", P);
mock.tick(2);
hook.remove();
mock.spawnItemEntity(OW, "minecraft:cod", 1, {x:P.x+10, y:P.y, z:P.z});
mock.tick(10);
return { items: mock.liveItems() };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:cod", 1)])

    def test_hook_in_other_dimension_untouched(self):
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity("minecraft:nether", "minecraft:fishing_hook", P);
mock.tick(2);
hook.remove();
mock.spawnItemEntity(OW, "minecraft:cod", 1, P);
mock.tick(10);
return { items: mock.liveItems(), nether: mock.liveItems("minecraft:nether") };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:cod", 1)])

    def test_late_item_outside_window_untouched(self):
        r = rt.run_scenario(HOOK + r"""
const hook = mock.createEntity(OW, "minecraft:fishing_hook", P);
mock.tick(2);
hook.remove();
mock.tick(30);
mock.spawnItemEntity(OW, "minecraft:cod", 1, P);
mock.tick(10);
return { items: mock.liveItems() };
""")
        self.assertEqual([(i["typeId"], i["amount"]) for i in r["items"]], [("minecraft:cod", 1)])


if __name__ == "__main__":
    unittest.main(verbosity=2)
