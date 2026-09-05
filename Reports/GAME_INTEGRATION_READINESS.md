# Native game-integration readiness

Date: 2026-09-04

## Decision

The native Genesis3D Linux port is ready for continued engine development and
asset inspection, but it is **not yet a complete runner for the legacy game**.

The separate Windows game remains runnable through its established Proton/Wine
compatibility path. Launching that game does not mean it is running inside the
new native engine.

## What the native runtime can do now

- Load a compatible compiled BSP map.
- Render world geometry, textures, baked lightmaps, and translucency.
- Load and pose compatible ACT/BODY actors.
- Advance a test actor's skeletal animation.
- Provide mouse look, WASD movement, collision, stairs, gravity, and jumping.
- Exercise dynamic-lightmap, frame-pacing, and teardown paths.

The present harness chooses one map, places one successfully parsed actor in
front of the camera, and provides a movable engine test scene. This proves core
runtime paths; it is not the legacy game's entity/gameplay loop.

## What is still missing for a native playable game

- Game bootstrap and level-flow logic.
- Entity spawning and placement from the game's authored data.
- Enemy AI and combat state.
- Player weapons, projectiles, damage, and effects.
- HUD, menus, inventory, objectives, and interaction prompts.
- Audio and music runtime services.
- Game scripts and triggers.
- Save/load compatibility.
- Game-specific video and UI presentation.
- A complete, legally controlled and sanitized asset set.

These belong primarily in a separate game/framework layer built on the engine,
not in the low-level Genesis3D renderer itself.

## Local asset warning

The untracked local purification workspace is not a finished sterile game-data
set. A few neutral fixture actors exist, but additional legacy filenames and
commercial assets remain locally. They are not tracked in the public engine
repository. They must stay private, be inventoried, and be either lawfully
retained and sanitized or replaced before any game project is published.

## Recommended path

1. Keep `Genesis3D_Project` as the reusable engine.
2. Create a separate private game-layer project beside it.
3. Define a manifest for maps, actors, entities, sounds, and scripts.
4. Port the smallest complete loop first: spawn, movement, one interaction,
   one enemy, one weapon, HUD, audio, and level exit.
5. Add automated fixtures for each gameplay system before expanding content.
6. Replace or sanitize legacy content without publishing third-party assets.
7. Perform a full human playthrough before calling the game playable.

## Verification basis

This conclusion comes from the active `linux_main.cpp`, the engine's recorded
verification scope, the community roadmap, and a read-only local asset
inventory. The engine or game was not launched for this assessment, and no
input device was captured.
