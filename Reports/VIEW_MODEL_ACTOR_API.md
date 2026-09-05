# Generic View-Model Actor API

## Boundary

Render API version 19 adds two generic actor operations:

- `geLinuxRender_SetActorTransformEuler` submits a finite world position and
  bounded X/Y/Z Euler rotation for any application-owned actor.
- `geLinuxRender_SetActorViewModel` classifies an actor for post-world rendering.

No weapon rules, filenames, offsets, animation names, or game data are present
in the public engine.

## Render ordering

Ordinary actors retain the existing BSP/world actor path. View-model actors are
excluded from that pass, then submitted after world geometry and transient
effects. The renderer clears only the depth buffer before the first view actor,
so the actor preserves its own surface depth while avoiding intersection with
nearby BSP geometry. That pass uses the camera field of view with a dedicated
0.01-unit near plane, preserving authored camera-local offsets instead of the
BSP world's radius-derived near plane. HUD/overlay callbacks remain last.

The actor at slot zero is no longer implicitly treated as a world actor when it
is marked as a view model. This keeps map replacement and actor-list ordering
safe for application-owned populations.

## Validation contract

Both new calls reject null runtimes, missing actor slots, invalid vectors,
non-finite angles, out-of-policy angle magnitudes, and non-boolean view flags.
Actor ownership and teardown remain unchanged.

Actor scale submission updates both body vertex scale and pose attachment/
motion translation scale. Applying only the body scale leaves small actors'
bone-bound pieces at unscaled offsets; the native API now follows the complete
legacy actor scale contract.

Actor materials use binary alpha rejection for legacy color-keyed texels.
Framebuffer capture redraws the current scene into the owned back buffer before
readback, so it does not depend on GLX preserving a post-swap front buffer.
