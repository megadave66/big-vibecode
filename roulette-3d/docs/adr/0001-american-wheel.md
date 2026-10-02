# ADR 0001 — American wheel

Status: accepted.

## Decision
The game uses an American wheel: 38 pockets (1–36, 0, 00).

## Why
The user chose it over the usual European default.

## Effects
- 38 pockets. The pocket ring follows the real American order (`scripts/core/wheel_layout.gd`).
- 00 is stored as the int 37 and shown as "00".
- The layout gains the 0-00 split, the trios 0-1-2, 0-00-2, 00-2-3, and the five-number bet (0, 00, 1, 2, 3) at 6:1.
- House edge is 5.26% (7.89% on the five-number bet).
- Everything else follows default casino rules.
