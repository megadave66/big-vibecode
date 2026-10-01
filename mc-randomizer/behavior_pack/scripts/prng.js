/**
 * Pure PRNG and hashing functions.
 * All algorithms must match the Python port in tools/remap_engine.py bit-for-bit.
 */

/**
 * FNV-1a 32-bit hash over UTF-16 code units.
 * @param {string} str
 * @returns {number} uint32
 */
export function hashString(str) {
  let h = 0x811c9dc5;
  for (let i = 0; i < str.length; i++) {
    const c = str.charCodeAt(i);
    h ^= c;
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h >>> 0;
}

/**
 * Mulberry32 PRNG factory.
 * @param {number} seed - uint32
 * @returns {() => number} RNG returning float in [0, 1)
 */
export function mulberry32(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/**
 * Random integer in [0, n).
 * @param {() => number} rng - mulberry32-style RNG
 * @param {number} n
 * @returns {number} integer in [0, n)
 */
export function randInt(rng, n) {
  return Math.floor(rng() * n);
}
