/**
 * Domain hooks: block drops and mob drops.
 * Both only record a "source"; the shared interceptor does the remapping.
 */

import { world } from "@minecraft/server";
import { EXCLUDED_ENTITIES } from "../remap_engine.js";

const PLAYER_ID = "minecraft:player";

/**
 * Non-empty container slots of a block, as skip entries.
 * @param {import("@minecraft/server").Block} block
 */
function containerContents(block) {
  const skip = [];
  try {
    const inv = /** @type {import("@minecraft/server").BlockInventoryComponent|undefined} */ (block.getComponent("minecraft:inventory"));
    const container = inv && inv.container;
    if (!container) return skip;
    for (let i = 0; i < container.size; i++) {
      const stack = container.getItem(i);
      if (stack) skip.push({ typeId: stack.typeId, amount: stack.amount });
    }
  } catch (err) {
    // No readable container: nothing to skip.
  }
  return skip;
}

/**
 * @param {{interceptor: any, log: (m: string) => void, [k: string]: any}} ctx
 */
export function install(ctx) {
  // beforeEvents.playerBreakBlock: the block and its container are still intact,
  // so the contents can be read here. The hook changes no world state.
  world.beforeEvents.playerBreakBlock.subscribe((ev) => {
    try {
      const block = ev.block;
      const c = block.center();
      ctx.interceptor.recordSource({
        domain: "block_drops",
        dimensionId: block.dimension.id,
        location: { x: c.x, y: c.y, z: c.z },
        radius: 1.5,
        ticks: 3,
        skip: containerContents(block),
      });
    } catch (err) {
      ctx.log("playerBreakBlock hook failed: " + (err && err.message));
    }
  });

  world.afterEvents.entityDie.subscribe((ev) => {
    try {
      const dead = ev.deadEntity;
      const typeId = dead.typeId;
      if (typeId === PLAYER_ID || EXCLUDED_ENTITIES.includes(typeId)) return;
      const loc = dead.location;
      ctx.interceptor.recordSource({
        domain: "mob_drops",
        dimensionId: dead.dimension.id,
        location: { x: loc.x, y: loc.y, z: loc.z },
        radius: 3,
        ticks: 25,
        skip: [],
      });
    } catch (err) {
      ctx.log("entityDie hook failed: " + (err && err.message));
    }
  });
}
