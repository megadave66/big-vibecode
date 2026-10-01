/**
 * mc-randomizer main entry point.
 * Wires the PRNG, seed derivation, remapping engine, and domain hooks.
 */

import { world, system } from "@minecraft/server";
import { createRemapper } from "./remap_engine.js";
import { ITEM_UNIVERSE } from "./item_universe.js";
import { getWorldSeed } from "./seed.js";
import { createInterceptor } from "./drop_interceptor.js";
import { install as installBlocksMobs } from "./hooks/blocks_mobs.js";
import { install as installFishing } from "./hooks/fishing.js";

/**
 * Logging utility with namespace prefix.
 * @param {string} msg
 */
function log(msg) {
  console.warn("[mc-randomizer] " + msg);
}

// Context shared across all hooks; initialized on world load.
let ctx = null;

/**
 * Initialize the randomizer on world load.
 * Guard against multiple runs.
 */
function initializeRandomizer() {
  try {
    if (ctx !== null) {
      log("Already initialized, skipping duplicate init.");
      return;
    }

    const seed = getWorldSeed();
    const remapper = createRemapper(seed, ITEM_UNIVERSE);
    const interceptor = createInterceptor(remapper);

    ctx = {
      seed,
      remapper,
      interceptor,
      log,
    };

    log(`Initialized with seed ${seed >>> 0}`);

    // Install domain hooks.
    installBlocksMobs(ctx);
    installFishing(ctx);

    log("All hooks installed.");
  } catch (err) {
    console.error("[mc-randomizer] Initialization failed:", err);
    throw err;
  }
}

/**
 * On world initialize, set up the randomizer.
 */
world.afterEvents.worldInitialize.subscribe(() => {
  initializeRandomizer();
});

/**
 * On player spawn (initial spawn only), notify the player of the seed.
 */
world.afterEvents.playerSpawn.subscribe((ev) => {
  try {
    if (ev.initialSpawn && ctx) {
      const player = ev.player;
      player.sendMessage(`[mc-randomizer] seed ${ctx.seed >>> 0}`);
    }
  } catch (err) {
    // Handle before init: ctx not yet ready.
  }
});

/**
 * On script event "mcr:info", send mapping info to the world.
 */
system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id !== "mcr:info") {
    return;
  }

  try {
    if (!ctx) {
      world.sendMessage("[mc-randomizer] Not yet initialized. Wait for world load.");
      return;
    }

    const itemsToQuery = [
      "minecraft:dirt",
      "minecraft:white_wool",
      "minecraft:mutton",
      "minecraft:cod",
      "minecraft:rotten_flesh",
    ];

    let msgLines = [];
    msgLines.push(`[mc-randomizer] Seed: ${ctx.seed >>> 0}`);
    msgLines.push("Mappings:");

    for (const itemId of itemsToQuery) {
      const mapped = ctx.remapper.map(itemId);
      msgLines.push(`  ${itemId} → ${mapped || "(not in universe)"}`);
    }

    msgLines.push(
      "Note: Crafting, smelting, chest loot, and trades use the fixed build seed (see build/remap_report.json)."
    );

    world.sendMessage(msgLines.join("\n"));
  } catch (err) {
    console.error("[mc-randomizer] Script event handler failed:", err);
  }
});
