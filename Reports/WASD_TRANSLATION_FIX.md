# Native WASD Translation Fix

Date: 2026-09-03

## Result

Native camera translation now uses explicit held-action state populated by
`SDL_KEYDOWN` and `SDL_KEYUP`. It no longer depends on
`SDL_GetKeyboardState()`, whose global snapshot remained empty for movement
keys after SDL attached to the already-created X11/GLX window with
`SDL_CreateWindowFrom`. This matches the physical observation that SDL mouse
motion events reached the same foreign window while WASD produced no camera
translation.

W/S and Up/Down independently control forward/backward movement; A/D and
Left/Right independently control strafing; either Shift enables the existing
2x sprint multiplier. Multiple aliases can be held and released independently.
Input repeat is idempotent, focus loss clears every held action, and diagonal
movement remains normalized.

Each fixed simulation step now consumes the explicit action state and applies
`move_speed * sprint * fixed_dt` to the camera's world-space X/Z position. The
render `CameraBasis` is derived after all fixed steps, ensuring BSP/PVS
selection and the OpenGL view receive the updated camera in the same frame.

## Deterministic evidence

The executable runs a narrow camera regression before graphics startup and
fails startup if it does not pass. It verifies:

- 60 fixed steps at 60 Hz move exactly 400 world units at normal speed.
- 120 presentation frames feeding the 60 Hz accumulator produce the same 400
  world units.
- A forward/right diagonal travels the same distance as a cardinal direction.
- Arrow-key aliases and Shift produce the expected 800-units/second sprint.

Observed result:

```text
Genesis3D Linux: deterministic camera movement: 60 Hz PASS, 120 Hz PASS, diagonal PASS, aliases/sprint PASS
```

The production build completed at 100%. A non-capturing eDP-1 hardware run
completed 253 frames at 60.00 FPS with zero schedule resynchronizations and no
render deadline misses. The Clang ASan+UBSan build completed at 100%; its
headless regression run exited cleanly with no sanitizer diagnostics. A
non-capturing graphics sanitizer run also reported no address or undefined
behavior fault during rendering; LeakSanitizer retained one 128-byte
process-global allocation below X11/SDL/driver frames at shutdown, outside the
camera path.

Every automated graphics launch set `GENESIS3D_NO_INPUT_CAPTURE=1`,
`GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080`. No automated launch
enabled input capture, requested focus, or captured the pointer. No engine
process remains running.

Implementation commit: `bcd5b2d`.

## User retest

```bash
cd /home/user/Genesis3D_Project && GENESIS3D_ENABLE_INPUT_CAPTURE=1 ./genesis3d_linux
```

Hold W/A/S/D and the arrow keys to verify translation, hold Shift to verify
sprint, move the mouse to verify look, and press Escape to exit.
