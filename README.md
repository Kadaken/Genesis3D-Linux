# Genesis3D
Open Source 3D Game Development Engine

This version of the Genesis3D engine is based on Genesis3D Version 1.1 released
November 15, 1999, by WildTangent, Inc. and includes numerous modifications and
improvements contributed by various authors.

## Native Linux asset layout

`genesis3d_linux` selects the alphabetically first readable compiled level from
`Assets/Maps/` (`.bsp` or `.gbsp`) and validates that `Assets/Actors/` contains
at least one readable `.act` character file. It reports the exact search path
and exits before creating an X11 window when either required asset is missing.

Set `GENESIS3D_PROJECT_ROOT` to override the default
`/home/user/Genesis3D_Project/PurificationWorkspace` asset root. The legacy
external asset links were removed after the sterile copy was validated; normal
launches now resolve only the isolated, project-owned asset tree.

## Native Linux controls

The SDL2-owned X11/OpenGL renderer uses SDL2 for native input. Interactive play must opt in
with `GENESIS3D_ENABLE_INPUT_CAPTURE=1`. Hold W/A/S/D or the arrow keys to
move, hold Shift to sprint, and move the mouse to look. Press Escape or close
the window to exit. With interactive input enabled, the mouse is captured
while the window has focus and released when focus is lost.

Input capture is disabled by default. `GENESIS3D_NO_INPUT_CAPTURE=1` is a hard
override: the engine keeps its SDL window hidden, does not force keyboard
focus, grab or confine the pointer, or enable relative mouse mode, and also
advertises an X11 non-input window hint. Agent-run tests must never set
`GENESIS3D_ENABLE_INPUT_CAPTURE`. The safe default window origin is the built-in
eDP-1 panel at `892,1080`; `GENESIS3D_WINDOW_X` and `GENESIS3D_WINDOW_Y` may set
a different initial position without preventing later movement.

The renderer initializes the camera at the map's `PlayerSetup` origin (falling
back to a bounds-derived overview only when that entity is absent), so sealed
BSP maps open from their playable interior rather than showing their unlit
outer hull. Material and lightmap textures are combined in one multitexture
pass, with tightly packed RGB lightmaps uploaded using one-byte row alignment.
Immutable BSP face geometry and both UV streams are cached in a static OpenGL
vertex buffer. Actor motion and light-style time advance on a canonical 60 Hz
fixed simulation clock, independently of 60/120 Hz presentation. Styled and
dynamic light layers regenerate only dirty, visible lightmap textures with
`glTexSubImage2D`; geometry is never rebuilt. Frame presentation is regulated with monotonic
`sleep_until` deadlines at either 60 or 120 FPS. SIGINT and SIGTERM request a
normal shutdown so graphics, world, SDL, X11, and VFile resources are released.

The native target treats pointer/integer narrowing, implicit C declarations,
and incompatible C pointer arguments as build errors. Audit methodology and
current validation limits are recorded in `PQPW_ARCHITECTURAL_AUDIT.md`.

LeakSanitizer and Valgrind suppressions for the Fedora SDL2-compat/Mesa driver
shutdown globals live in `tools/sanitizers/`. They are intentionally scoped to
library frames and must not be broadened to engine call sites.
