# Native trace API boundary

Date: 2026-09-05

## Problem

The private game layer needs weapon and interaction raycasts, but BSP internals
must remain owned by the reusable engine. Reimplementing BSP traversal in the
game would duplicate unsafe legacy-format knowledge and couple the game to
engine structures.

## Solution

Render API version 2 adds `geLinuxRender_TraceWorld`. The call accepts finite
world-space endpoints and returns API success independently from hit/miss. A hit
contains a normalized engine hit category, impact point, surface normal, and
segment fraction; it never exposes engine-owned pointers across the shared
library boundary.

The input snapshot also exposes the held state of SDL's left mouse button as
`Fire`. Focus loss clears the held state, preventing a stuck-fire condition.
The engine contains no ammo, damage, weapon, enemy, or other game rules.

## Verification

- Engine and standalone runtime build: PASS.
- Shared-library export for `geLinuxRender_TraceWorld`: PASS.
- Real BSP forward trace from the authored player start: PASS.
- Private game-layer left-button edge, ammo decrement, impact logging: PASS.
- Clang ASan/UBSan trace lifecycle: PASS.
- Graphical window and input capture during automated tests: disabled.
