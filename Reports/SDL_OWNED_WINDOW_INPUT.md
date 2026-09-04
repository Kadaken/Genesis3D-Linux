# SDL-Owned Window Input

Date: 2026-09-03

## Result

The Linux renderer now uses an SDL-owned X11 window and OpenGL context from
creation through teardown. This removes the `SDL_CreateWindowFrom` foreign
window boundary that prevented SDL's XWayland backend from establishing normal
keyboard focus and consequently discarded physical `SDL_KEYDOWN` and
`SDL_KEYUP` delivery.

The render loop's single event consumer remains `SDL_PollEvent`. Its key events
feed the explicit held-action state for W/A/S/D, arrows, and both Shift keys;
the 60 Hz simulation then applies `move_speed * sprint * fixed_dt`, and the
post-simulation `CameraBasis` reaches PVS selection and the OpenGL view in the
same frame.

## Migration

- `SDL_CreateWindow(..., SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)` owns the
  native window. The interactive opt-in shows it; non-capturing execution keeps
  it hidden and nonactivating.
- SDL requests 8-bit RGB, a 24-bit depth buffer, double buffering, and an
  OpenGL 2.1-compatible context. The runtime selected the AMD Radeon/Mesa 4.6
  compatibility renderer.
- Context lifecycle is exclusively `SDL_GL_CreateContext`,
  `SDL_GL_MakeCurrent`, `SDL_GL_SwapWindow`, and `SDL_GL_DeleteContext`.
- Raw X11/GLX window, colormap, visual, context, mapping, swapping, and event
  lifecycle code was removed. A narrow SDL native-window query remains only to
  set the X11 `InputHint=False` safety hint for unattended runs; it does not
  create resources or consume events.
- Drawable size is queried with `SDL_GL_GetDrawableSize` at startup and after
  resize events. Window placement is reported with `SDL_GetWindowPosition`.
- `GENESIS3D_NO_INPUT_CAPTURE=1` wins over the interactive opt-in. It keeps the
  window hidden, never requests focus, window grab, or relative mouse mode, and
  clears held state on focus loss. Interactive input remains available only
  through `GENESIS3D_ENABLE_INPUT_CAPTURE=1`.

## Verification

The startup regression injects W `SDL_KEYDOWN` and `SDL_KEYUP` through
`SDL_PushEvent`, consumes them through the same `SDL_PollEvent` API as the
runtime, and verifies the held state sets and clears. The existing deterministic
camera checks still pass at 60 and 120 Hz, including diagonal normalization,
aliases, and sprint.

- Production Release build: pass.
- SDL-owned window/context and keyboard queue regression: pass.
- Four actor fixture parse, pose, animation, and render checks: pass.
- Dynamic light add and byte-exact remove/restore cycle: pass.
- 60 Hz Release run: 269 frames / 4.483 s, 60.00 FPS, zero schedule
  resynchronizations and zero render deadline misses.
- 120 Hz Release run: 769 frames / 6.408 s, 120.00 FPS, zero schedule
  resynchronizations, 16 render deadline misses, and 4.594 ms maximum lateness.
- Clang ASan+UBSan build and non-capturing graphics run: pass with no address or
  undefined-behavior diagnostics; 280 frames / 4.667 s at 60.00 FPS with zero
  schedule resynchronizations and zero render deadline misses.
- Startup diagnostics confirmed requested position `892,1080`, drawable
  `1920x1080`, and the hardware AMD Radeon renderer.

Every live validation used `GENESIS3D_NO_INPUT_CAPTURE=1`,
`GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080`. No validation enabled
input capture, showed or focused the test window, grabbed input, or entered
relative mouse mode. No Genesis3D process remains.

Implementation commit: `59249b4`.

## Physical retest

The remaining check is physical keyboard and mouse behavior on the SDL-owned
XWayland window:

```bash
cd /path/to/Genesis3D-Linux && GENESIS3D_ENABLE_INPUT_CAPTURE=1 ./build/genesis3d_linux
```

Hold W/A/S/D and the arrow keys to verify translation, hold Shift to verify
sprint, move the mouse to verify look, resize once to verify the viewport, and
press Escape to release capture and exit.
