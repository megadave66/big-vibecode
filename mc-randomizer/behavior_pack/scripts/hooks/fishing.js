/**
 * Domain hook: fishing (runtime, per-world seed).
 *
 * There is no fishing or loot API in @minecraft/server 1.11.0. So we watch the
 * fishing hook entity. When a hook goes away (reel-in), we tell the shared drop
 * interceptor: "items that spawn near this spot in the next few ticks are a
 * fishing catch". The interceptor then swaps them for the mapped item.
 *
 * Two signals, one source per hook:
 * 1. Main: world.beforeEvents.entityRemove for "minecraft:fishing_hook". The
 *    entity is still valid here, so we read its exact location. (Before events
 *    are read-only for world state; we only write JS memory.)
 * 2. Fallback: a per-tick scan of live hooks keeps their last-known location.
 *    If a hook vanishes from the scan and no remove event was seen for it, we
 *    record the source from the last-known location instead.
 * The interceptor waits a few ticks for a late source, so the catch item can
 * spawn just before or just after the hook is removed.
 */

import { world, system } from "@minecraft/server";

export const FISHING_HOOK = "minecraft:fishing_hook";
/** The catch spawns at the hook, then flies to the player. 3 blocks covers it. */
export const CATCH_RADIUS = 3;
/** Ticks around the hook removal in which a spawned item counts as the catch. */
export const CATCH_TICKS = 5;
const DIMENSIONS = ["minecraft:overworld", "minecraft:nether", "minecraft:the_end"];
/** Ids of hooks we already recorded a source for, kept this long. */
const RECORDED_TTL_TICKS = 200;

/**
 * @param {{seed: number, remapper: any, interceptor: {recordSource(src: any): void},
 *   log(msg: string): void}} ctx runtime context from main.js
 */
export function install(ctx) {
  /** @type {Map<string, {dimensionId: string, location: {x:number,y:number,z:number}}>} */
  const live = new Map();
  /** @type {Map<string, number>} hook id -> tick its source was recorded */
  const recorded = new Map();

  /**
   * @param {string} id
   * @param {string} dimensionId
   * @param {{x:number,y:number,z:number}} location
   */
  function record(id, dimensionId, location) {
    if (recorded.has(id)) return;
    recorded.set(id, system.currentTick);
    live.delete(id);
    ctx.interceptor.recordSource({
      domain: "fishing",
      dimensionId,
      location: { x: location.x, y: location.y, z: location.z },
      radius: CATCH_RADIUS,
      ticks: CATCH_TICKS,
    });
  }

  world.beforeEvents.entityRemove.subscribe((ev) => {
    try {
      const e = ev.removedEntity;
      if (!e || e.typeId !== FISHING_HOOK) return;
      const id = e.id;
      let dimensionId;
      let location;
      try {
        dimensionId = e.dimension.id;
        location = e.location;
      } catch (err) {
        const last = live.get(id);
        if (!last) return;
        dimensionId = last.dimensionId;
        location = last.location;
      }
      record(id, dimensionId, location);
    } catch (err) {
      ctx.log("fishing entityRemove: " + (err && err.message ? err.message : err));
    }
  });

  system.runInterval(() => {
    try {
      /** @type {Set<string>} */
      const seen = new Set();
      for (const dimId of DIMENSIONS) {
        let hooks;
        try {
          hooks = world.getDimension(dimId).getEntities({ type: FISHING_HOOK });
        } catch (err) {
          continue;
        }
        for (const h of hooks) {
          if (!h.isValid()) continue;
          const loc = h.location;
          seen.add(h.id);
          live.set(h.id, { dimensionId: h.dimension.id, location: { x: loc.x, y: loc.y, z: loc.z } });
        }
      }
      // Fallback: a hook gone from the scan with no remove event.
      for (const [id, last] of live) {
        if (!seen.has(id)) record(id, last.dimensionId, last.location);
      }
      const now = system.currentTick;
      for (const [id, t] of recorded) {
        if (now - t > RECORDED_TTL_TICKS) recorded.delete(id);
      }
    } catch (err) {
      ctx.log("fishing scan: " + (err && err.message ? err.message : err));
    }
  }, 1);
}
