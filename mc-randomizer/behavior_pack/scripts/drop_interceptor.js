/**
 * Item-entity interception for the runtime domains (block drops, mob drops, ...).
 *
 * Flow: hooks call recordSource() when something that drops items happens.
 * Every item entity that spawns is buffered. On later ticks each buffered item
 * is matched to a source (same dimension, near, close in time). A match means
 * the item is removed and replaced by the mapped item(s), same total count.
 */

import { world, system, ItemStack } from "@minecraft/server";
import { splitCount } from "./remap_engine.js";

const ITEM_TYPE = "minecraft:item";
/** An item waits this many ticks for a source that is recorded after it spawned. */
const DEFAULT_GRACE_TICKS = 5;
/** Sources live this long past their own window. */
const SOURCE_SLACK_TICKS = 10;
/** Own-spawned ids are remembered this long. */
const OWN_ID_TTL_TICKS = 400;

/**
 * @typedef {{typeId: string, amount: number}} SkipEntry
 * @typedef {{domain: string, dimensionId: string, location: {x:number,y:number,z:number},
 *   radius: number, ticks: number, skip?: SkipEntry[]}} SourceInput
 * @typedef {{domain: string, dimensionId: string, location: {x:number,y:number,z:number},
 *   radius: number, ticks: number, skip: SkipEntry[], tick: number}} Source
 * @typedef {{entity: any, id: string, dimensionId: string,
 *   location: {x:number,y:number,z:number}, tick: number}} Buffered
 */

/**
 * @param {{map(id: string): (string|undefined)}} remapper
 * @param {{graceTicks?: number}} [options]
 */
export function createInterceptor(remapper, options) {
  const grace = (options && options.graceTicks) || DEFAULT_GRACE_TICKS;
  /** @type {Buffered[]} */
  let buffer = [];
  /** @type {Source[]} */
  let sources = [];
  /** @type {Map<string, number>} */
  const ownSpawned = new Map();
  const stats = { remapped: 0, skipped: 0, unmatched: 0, errors: 0, byDomain: /** @type {Record<string, number>} */ ({}) };

  function logError(msg, err) {
    console.warn("[mc-randomizer] interceptor: " + msg + " " + (err && err.message ? err.message : err));
  }

  world.afterEvents.entitySpawn.subscribe((ev) => {
    try {
      const entity = ev.entity;
      if (!entity || entity.typeId !== ITEM_TYPE) return;
      if (ev.cause === "Loaded") return;
      if (ownSpawned.has(entity.id)) return;
      const loc = entity.location;
      buffer.push({
        entity,
        id: entity.id,
        dimensionId: entity.dimension.id,
        location: { x: loc.x, y: loc.y, z: loc.z },
        tick: system.currentTick,
      });
    } catch (err) {
      logError("spawn handler", err);
    }
  });

  /** @param {SourceInput} src */
  function recordSource(src) {
    sources.push({
      domain: src.domain,
      dimensionId: src.dimensionId,
      location: { x: src.location.x, y: src.location.y, z: src.location.z },
      radius: src.radius,
      ticks: src.ticks,
      skip: (src.skip || []).map((s) => ({ typeId: s.typeId, amount: s.amount })),
      tick: system.currentTick,
    });
  }

  /** @param {Buffered} item @returns {Source|undefined} nearest matching source */
  function findSource(item) {
    let best;
    let bestDist = Infinity;
    for (const s of sources) {
      if (s.dimensionId !== item.dimensionId) continue;
      if (Math.abs(item.tick - s.tick) > s.ticks) continue;
      const dx = item.location.x - s.location.x;
      const dy = item.location.y - s.location.y;
      const dz = item.location.z - s.location.z;
      const d = Math.sqrt(dx * dx + dy * dy + dz * dz);
      if (d > s.radius) continue;
      if (d < bestDist) {
        bestDist = d;
        best = s;
      }
    }
    return best;
  }

  /** Handle one matched item. */
  function remapItem(item, source) {
    const entity = item.entity;
    if (!entity.isValid()) return;
    const stack = entity.getComponent("minecraft:item").itemStack;
    const skipAt = source.skip.findIndex((s) => s.typeId === stack.typeId && s.amount === stack.amount);
    if (skipAt >= 0) {
      source.skip.splice(skipAt, 1);
      stats.skipped++;
      return;
    }
    const to = remapper.map(stack.typeId);
    if (to === undefined) return;
    const maxAmount = new ItemStack(to, 1).maxAmount;
    const dimension = entity.dimension;
    const spawnedEntities = [];
    try {
      for (const count of splitCount(stack.amount, maxAmount)) {
        const spawned = dimension.spawnItem(new ItemStack(to, count), item.location);
        ownSpawned.set(spawned.id, system.currentTick);
        spawnedEntities.push(spawned);
      }
    } catch (err) {
      // Partial spawn: keep the original so nothing is lost, remove the extras.
      for (const e of spawnedEntities) {
        try { e.remove(); } catch (e2) { /* already gone */ }
      }
      stats.errors++;
      logError("spawn failed for " + to, err);
      return;
    }
    entity.remove();
    stats.remapped++;
    stats.byDomain[source.domain] = (stats.byDomain[source.domain] || 0) + 1;
  }

  function step() {
    const now = system.currentTick;
    const keep = [];
    for (const item of buffer) {
      const age = now - item.tick;
      if (age < 1) {
        keep.push(item);
        continue;
      }
      try {
        const source = findSource(item);
        if (source) {
          remapItem(item, source);
        } else if (age <= grace) {
          keep.push(item);
        } else {
          stats.unmatched++;
        }
      } catch (err) {
        stats.errors++;
        logError("item", err);
      }
    }
    buffer = keep;
    sources = sources.filter((s) => now - s.tick <= s.ticks + SOURCE_SLACK_TICKS + grace);
    for (const [id, t] of ownSpawned) {
      if (now - t > OWN_ID_TTL_TICKS) ownSpawned.delete(id);
    }
  }

  system.runInterval(() => {
    try {
      step();
    } catch (err) {
      stats.errors++;
      logError("interval", err);
    }
  }, 1);

  return {
    recordSource,
    stats,
    pendingCount: () => buffer.length,
    sourceCount: () => sources.length,
  };
}
