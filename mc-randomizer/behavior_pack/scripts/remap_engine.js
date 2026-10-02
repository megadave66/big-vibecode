/**
 * Remap engine. Pure: no Minecraft imports, so node can test it.
 * Must match tools/remap_engine.py bit for bit.
 */
import { mulberry32, randInt } from "./prng.js";

/** All 7 randomized domains. One permutation is shared by all of them. */
export const DOMAINS = Object.freeze([
  "block_drops",
  "mob_drops",
  "crafting",
  "smelting",
  "chest_loot",
  "fishing",
  "trades",
]);

/** Entities whose drops are never remapped. */
export const EXCLUDED_ENTITIES = Object.freeze(["minecraft:ender_dragon"]);

/**
 * @param {string} typeId
 * @returns {boolean}
 */
export function isExcludedEntity(typeId) {
  return EXCLUDED_ENTITIES.indexOf(typeId) !== -1;
}

/**
 * Sattolo shuffle over the sorted, deduped universe: one n-cycle, so no item maps to itself.
 * @param {number} seed uint32
 * @param {string[]} universe
 * @returns {Record<string, string>} plain object {src: dst}
 */
export function buildPermutation(seed, universe) {
  const sorted = Array.from(new Set(universe)).sort();
  const arr = sorted.slice();
  const rng = mulberry32(seed);
  for (let i = arr.length - 1; i >= 1; i--) {
    const j = randInt(rng, i);
    const tmp = arr[i];
    arr[i] = arr[j];
    arr[j] = tmp;
  }
  /** @type {Record<string, string>} */
  const map = {};
  for (let k = 0; k < sorted.length; k++) map[sorted[k]] = arr[k];
  return map;
}

/**
 * @param {number} seed uint32
 * @param {string[]} universe
 * @returns {{ mapping: Record<string, string>, map: (id: string) => (string | undefined) }}
 */
export function createRemapper(seed, universe) {
  const mapping = buildPermutation(seed, universe);
  return {
    mapping,
    map(id) {
      return Object.prototype.hasOwnProperty.call(mapping, id) ? mapping[id] : undefined;
    },
  };
}

/**
 * Full stacks of maxStack, then the remainder. [] for total <= 0.
 * @param {number} total
 * @param {number} maxStack
 * @returns {number[]}
 */
export function splitCount(total, maxStack) {
  /** @type {number[]} */
  const out = [];
  if (!Number.isFinite(total) || !(total > 0)) return out;
  const mx = Number.isFinite(maxStack) ? Math.max(1, Math.floor(maxStack)) : 1;
  let left = Math.floor(total);
  while (left >= mx) {
    out.push(mx);
    left -= mx;
  }
  if (left > 0) out.push(left);
  return out;
}
