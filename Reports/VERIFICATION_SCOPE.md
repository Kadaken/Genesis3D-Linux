# Genesis3D Linux Verification Scope

Date: 2026-09-03

## Short answer

The native port's implemented core systems have been exercised and pass their
documented tests. That does not mean every subsystem from the historical
Genesis3D SDK, RealityFactory, or the original game has been ported and tested.
The current deliverable is a working native engine sandbox and render/physics
harness, not a feature-complete game.

## What is verified

| Capability | Evidence level | Result |
|---|---|---|
| Native x86-64 C/C++ build | Production CMake builds | PASS |
| Memory/type safety | Clang ASan and UBSan runs | PASS for engine-owned paths; one documented external 128-byte driver/global record remains |
| BSP loading | Real purified `rfedit1.bsp` | PASS |
| Textures and baked lightmaps | Hardware-rendered compositor capture | PASS visually; exact pixel equivalence lacks an original-renderer reference image |
| PVS selection and translucent ordering | Runtime counters and hardware rendering | PASS as implemented |
| SDL window and OpenGL ownership | Native XWayland hardware runs | PASS |
| Mouse look and planar WASD | User physical test after SDL migration | PASS, confirmed by Kadaken |
| Actor BODY/ACT parsing | Four purified real actor files | PASS |
| Skeletal animation | Pose-time advancement and measured posed-vertex changes on all four actors | PASS |
| Runtime dynamic lightmaps | Real lightmap byte changes through controlled add/remove fixture | PASS; removal restores baked bytes exactly |
| BSP collision and movement physics | Deterministic tests against real `rfedit1.bsp` | PASS |
| Stairs, slopes, gravity, jumping and ceilings | Real-map automated collision regressions | PASS structurally; physical gameplay feel has not yet been confirmed by Kadaken |
| Fixed simulation pacing | 60/120 Hz and forced one-second stall tests | PASS; catch-up clamps to 250 ms |
| Hardware cadence | Long AMD/Mesa runs | PASS at 60.00 and 120.00 FPS, with small compositor/scheduler deadline jitter documented |
| Signal teardown | SIGINT/SIGTERM and sanitizer runs | PASS for engine-owned resources |

## Verified through controlled fixtures, not authored map content

`rfedit1.bsp` contains no configured active styled surfaces and no persistent
dynamic lights. The runtime light path was therefore tested by adding and
removing an actual engine dynamic light and checking the resulting GPU-bound
lightmap bytes. This validates the implementation, but it is not a visual test
of a designer-authored dynamic-light scene.

Likewise, actor parsing, materials, pose changes, and primitive submission are
validated. Human visual review of every animation and material from every
camera angle has not been performed.

## Known incomplete or unverified areas

- Exact visual equivalence with the original Windows renderer: no matched
  reference capture exists.
- Area-portal traversal and full legacy polygon frustum clipping beyond the
  implemented cluster PVS path.
- BSP trace-based shadow occlusion for dynamic lights.
- A designer-authored map containing animated light styles and persistent
  dynamic lights.
- Physical tuning of hull dimensions, step height, gravity, and jump impulse.
- The legacy BODY writer/export pipeline; the native reader/runtime path is the
  part currently repaired.
- Complete game systems such as menus, HUD, audio, weapons, enemy AI,
  objectives, scripting, save/load, networking, and content authoring tools.

## Responsible project description

It is accurate to call this a tested, working native Linux runtime port and
modernized engine sandbox for Genesis3D 1.1 assets. It is not yet accurate to
call it a complete port of every Genesis3D tool or a complete playable port of
the legacy game.
