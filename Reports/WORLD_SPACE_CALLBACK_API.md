# World-Space Callback API

Date: 2026-09-05  
API version: 20

## Purpose

The native shared renderer now exposes a generic world-space callback for
application-owned presentation such as temporary surface marks. It contains no
game-specific entity definitions, asset paths, or content policy.

`geLinuxRender_SetWorldCallback` registers a callback on the opaque runtime.
The callback runs after visible BSP geometry and before world actors while the
active camera projection and model-view matrices remain available.

## State boundary

The renderer pushes all OpenGL attributes plus the projection and model-view
matrices before invoking application code, then restores them afterward. This
keeps application drawing from changing the actor, transient-effect, view-model,
or HUD passes. A null callback clears the registration.

## Verification

- Clean GCC engine build and shared-library link: PASS.
- Private consumer compiled against API version 20: PASS.
- Isolated Xvfb world-space callback invocation and state restoration: PASS.
- No physical display or desktop input capture was used.
