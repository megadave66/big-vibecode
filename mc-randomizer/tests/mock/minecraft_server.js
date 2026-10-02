// Node mock of @minecraft/server (1.11.0 subset) for static runtime tests.
// Reusable: add scenarios/helpers additively. Everything test-only lives under `mock`.

class Signal {
  constructor() { this.cbs = []; }
  subscribe(cb) { this.cbs.push(cb); return cb; }
  unsubscribe(cb) { this.cbs = this.cbs.filter((c) => c !== cb); }
  fire(ev) { for (const cb of this.cbs.slice()) cb(ev); }
}

let nextEntityId = 1;
const state = {
  tick: 0,
  intervals: [],   // {fn, every, next, id}
  timeouts: [],    // {fn, at, id}
  nextHandle: 1,
  queued: [],      // spawn events: {at, ev}
  dynProps: new Map(),
  dimensions: new Map(),
  players: [],
  messages: [],
};

export class ItemStack {
  constructor(itemType, amount = 1) {
    const typeId = typeof itemType === "string" ? itemType : itemType.id;
    if (typeof typeId !== "string" || !/^[a-z0-9_.]+:[a-z0-9_./]+$/.test(typeId)) {
      throw new Error(`Invalid item type: ${typeId}`);
    }
    if (!Number.isInteger(amount) || amount < 1 || amount > 255) {
      throw new Error(`Invalid amount: ${amount}`);
    }
    this.typeId = typeId;
    this.amount = amount;
  }
  get maxAmount() {
    const id = this.typeId;
    if (/(sword|pickaxe|_axe|shovel|hoe|helmet|chestplate|leggings|boots|bow)$/.test(id)) return 1;
    if (/(ender_pearl|snowball|_egg|^minecraft:egg$|_sign|bucket)/.test(id) || id === "minecraft:egg") return 16;
    return 64;
  }
}

class MockEntity {
  constructor(dimension, typeId, location) {
    this.id = String(nextEntityId++);
    this.typeId = typeId;
    this.dimension = dimension;
    this.location = { x: location.x, y: location.y, z: location.z };
    this._valid = true;
    this._components = new Map();
  }
  isValid() { return this._valid; }
  getComponent(id) { return this._components.get(id); }
  remove() {
    if (!this._valid) throw new Error("Entity is not valid");
    mock.fireBefore("entityRemove", { removedEntity: this });
    this._valid = false;
    this.dimension._entities.delete(this.id);
  }
}

class MockDimension {
  constructor(id) { this.id = id; this._entities = new Map(); }
  _add(e) { this._entities.set(e.id, e); return e; }
  getEntities(opts = {}) {
    return [...this._entities.values()].filter((e) =>
      (!opts.type || e.typeId === opts.type));
  }
  spawnItem(stack, location) {
    return mock.spawnItemEntity(this.id, stack.typeId, stack.amount, location, "Spawned");
  }
}

class MockBlock {
  constructor(dimension, typeId, location, contents) {
    this.dimension = dimension;
    this.typeId = typeId;
    this.location = { x: location.x, y: location.y, z: location.z };
    this._contents = contents || null; // array of {typeId, amount} | null
  }
  center() { return { x: this.location.x + 0.5, y: this.location.y + 0.5, z: this.location.z + 0.5 }; }
  getComponent(id) {
    if (id === "minecraft:inventory" && this._contents) {
      const items = this._contents;
      return {
        container: {
          size: Math.max(27, items.length),
          getItem: (i) => (items[i] ? new ItemStack(items[i].typeId, items[i].amount) : undefined),
        },
      };
    }
    return undefined;
  }
}

export const system = {
  get currentTick() { return state.tick; },
  runInterval(fn, every = 1) {
    const id = state.nextHandle++;
    state.intervals.push({ fn, every, next: state.tick + every, id });
    return id;
  },
  runTimeout(fn, delay = 1) {
    const id = state.nextHandle++;
    state.timeouts.push({ fn, at: state.tick + delay, id });
    return id;
  },
  run(fn) { return system.runTimeout(fn, 1); },
  clearRun(id) {
    state.intervals = state.intervals.filter((i) => i.id !== id);
    state.timeouts = state.timeouts.filter((t) => t.id !== id);
  },
  afterEvents: { scriptEventReceive: new Signal() },
};

export const world = {
  afterEvents: {
    worldInitialize: new Signal(),
    playerSpawn: new Signal(),
    playerBreakBlock: new Signal(),
    entityDie: new Signal(),
    entitySpawn: new Signal(),
    entityRemove: new Signal(),
  },
  beforeEvents: {
    playerBreakBlock: new Signal(),
    entityRemove: new Signal(),
  },
  getDimension(id) {
    const key = id.startsWith("minecraft:") ? id : "minecraft:" + id;
    if (!state.dimensions.has(key)) state.dimensions.set(key, new MockDimension(key));
    return state.dimensions.get(key);
  },
  getDynamicProperty(k) { return state.dynProps.get(k); },
  setDynamicProperty(k, v) { state.dynProps.set(k, v); },
  getAllPlayers() { return state.players.slice(); },
  sendMessage(m) { state.messages.push(m); },
};

// ---- test helpers (not part of the real API) ----
export const mock = {
  state,
  /** Advance n ticks: bump tick, deliver due spawn events, run timeouts, run intervals. */
  tick(n = 1) {
    for (let k = 0; k < n; k++) {
      state.tick++;
      const due = state.queued.filter((q) => q.at <= state.tick);
      state.queued = state.queued.filter((q) => q.at > state.tick);
      for (const q of due) {
        if (q.ev.entity._valid) world.afterEvents.entitySpawn.fire(q.ev);
      }
      const touts = state.timeouts.filter((t) => t.at <= state.tick);
      state.timeouts = state.timeouts.filter((t) => t.at > state.tick);
      for (const t of touts) t.fn();
      for (const i of state.intervals.slice()) {
        if (state.tick >= i.next) { i.next = state.tick + i.every; i.fn(); }
      }
    }
  },
  fireBefore(name, ev) { world.beforeEvents[name].fire(ev); },
  fireAfter(name, ev) { world.afterEvents[name].fire(ev); },
  /** Create an item entity. Its entitySpawn event is delivered next tick. */
  spawnItemEntity(dimId, typeId, amount, location, cause = "Spawned") {
    const dim = world.getDimension(dimId);
    const e = new MockEntity(dim, "minecraft:item", location);
    const stack = new ItemStack(typeId, amount);
    e._components.set("minecraft:item", { itemStack: stack });
    dim._add(e);
    state.queued.push({ at: state.tick + 1, ev: { entity: e, cause } });
    return e;
  },
  /** Create a non-item entity (mob, player). No spawn event. */
  createEntity(dimId, typeId, location) {
    const e = new MockEntity(world.getDimension(dimId), typeId, location);
    e.dimension._add(e);
    return e;
  },
  createPlayer(dimId = "minecraft:overworld", location = { x: 0, y: 64, z: 0 }) {
    const p = mock.createEntity(dimId, "minecraft:player", location);
    p.sendMessage = (m) => state.messages.push(m);
    state.players.push(p);
    return p;
  },
  createBlock(dimId, typeId, location, contents) {
    return new MockBlock(world.getDimension(dimId), typeId, location, contents);
  },
  /**
   * Player breaks a block. Order: beforeEvent, then the drops spawn, then afterEvent.
   * drops: [{typeId, amount}] (container contents are the caller's job to include).
   */
  breakBlock(player, block, drops = []) {
    const ev = { player, block, dimension: block.dimension, cancel: false };
    world.beforeEvents.playerBreakBlock.fire(ev);
    if (ev.cancel) return [];
    const spawned = drops.map((d) =>
      mock.spawnItemEntity(block.dimension.id, d.typeId, d.amount, block.center(), "Spawned"));
    world.afterEvents.playerBreakBlock.fire({
      player, block, dimension: block.dimension,
      brokenBlockPermutation: { type: { id: block.typeId } },
    });
    return spawned;
  },
  /** Entity dies: entityDie fires, then drops spawn at its location, then it is removed. */
  killEntity(entity, drops = []) {
    const loc = { ...entity.location };
    world.afterEvents.entityDie.fire({ deadEntity: entity, damageSource: { cause: "entityAttack" } });
    const spawned = drops.map((d) =>
      mock.spawnItemEntity(entity.dimension.id, d.typeId, d.amount, loc, "Spawned"));
    if (entity._valid) entity.remove();
    return spawned;
  },
  /** All live item entities as [{id, typeId, amount}] sorted by id. */
  liveItems(dimId = "minecraft:overworld") {
    return world.getDimension(dimId).getEntities({ type: "minecraft:item" })
      .map((e) => {
        const s = e.getComponent("minecraft:item").itemStack;
        return { id: e.id, typeId: s.typeId, amount: s.amount };
      });
  },
};

export default { world, system, ItemStack, mock };
