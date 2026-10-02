"""Python port of behavior_pack/scripts/{prng,remap_engine}.js.

Must match the JS bit for bit (checked by tests/test_remaps.py TestEngineParity).
32-bit JS semantics are emulated with explicit masking:
  x >>> 0      -> x & 0xFFFFFFFF
  x | 0        -> to_int32(x)
  Math.imul    -> imul(a, b)
Stdlib only.
"""

from __future__ import annotations

import math
from typing import Callable, Iterable

MASK = 0xFFFFFFFF

DOMAINS = ["block_drops", "mob_drops", "crafting", "smelting", "chest_loot", "fishing", "trades"]
EXCLUDED_ENTITIES = ["minecraft:ender_dragon"]


def to_int32(x: int) -> int:
    x &= MASK
    return x - 0x100000000 if x & 0x80000000 else x


def imul(a: int, b: int) -> int:
    """Math.imul: low 32 bits of the product, as signed int32."""
    return to_int32((a & MASK) * (b & MASK))


def hash_string(s: str) -> int:
    """FNV-1a 32-bit over UTF-16 code units (same as JS charCodeAt)."""
    h = 0x811C9DC5
    data = s.encode("utf-16-le")
    for k in range(0, len(data), 2):
        c = data[k] | (data[k + 1] << 8)
        h ^= c
        h = imul(h, 0x01000193) & MASK
    return h & MASK


def mulberry32(seed: int) -> Callable[[], float]:
    state = [seed & MASK]  # holds `a` (JS int32 after first step)

    def rng() -> float:
        a = to_int32(state[0] + 0x6D2B79F5)
        state[0] = a
        ua = a & MASK
        t = imul(ua ^ (ua >> 15), 1 | ua)
        ut = t & MASK
        t = to_int32((t + imul(ut ^ (ut >> 7), 61 | ut)) ^ t)
        ut = t & MASK
        return ((ut ^ (ut >> 14)) & MASK) / 4294967296

    return rng


def rand_int(rng: Callable[[], float], n: int) -> int:
    return math.floor(rng() * n)


def build_permutation(seed: int, universe: Iterable[str]) -> dict[str, str]:
    """Sattolo shuffle over the sorted, deduped universe: one n-cycle, no fixed points."""
    srt = sorted(set(universe))
    arr = list(srt)
    rng = mulberry32(seed)
    for i in range(len(arr) - 1, 0, -1):
        j = rand_int(rng, i)
        arr[i], arr[j] = arr[j], arr[i]
    return {srt[k]: arr[k] for k in range(len(srt))}


def split_count(total: int, max_stack: int) -> list[int]:
    if not math.isfinite(total) or not total > 0:
        return []
    mx = max(1, math.floor(max_stack)) if math.isfinite(max_stack) else 1
    full, rem = divmod(math.floor(total), mx)
    return [mx] * full + ([rem] if rem else [])


def is_excluded_entity(type_id: str) -> bool:
    return type_id in EXCLUDED_ENTITIES
