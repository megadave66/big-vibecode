import { world } from "@minecraft/server";
import { hashString } from "./prng.js";

/**
 * Retrieve or derive the world seed stored in dynamic property "mcr:seed".
 * Same world always returns the same seed.
 * Must only be called after world initialize event.
 *
 * @returns {number} uint32 world seed
 */
export function getWorldSeed() {
  const SEED_PROPERTY = "mcr:seed";
  let seed = world.getDynamicProperty(SEED_PROPERTY);

  if (typeof seed !== "number") {
    // Derive a new uint32 seed from current time and random value.
    seed = hashString(`${Date.now()}:${Math.random()}`);
    world.setDynamicProperty(SEED_PROPERTY, seed);
  }

  return seed >>> 0;
}
