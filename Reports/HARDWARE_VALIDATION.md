# Genesis3D Native Hardware Validation Report

Date: 2026-09-03

## Executive result

The native AMD GPU sustains the 60 FPS target at 59.86 FPS. It does not sustain
the 120 FPS target: the measured rate is 78.51 FPS and every measured frame
finished after its absolute render deadline. The fixed-step accumulator is
bounded correctly after a forced one-second scheduler stall. Both SIGINT and
SIGTERM enter normal teardown. The scoped VFile registry cleanup passes, but
SDL/GLX driver allocations prevent a blanket zero-process-leak claim. The live
visual gate fails because the BSP renders almost entirely black despite the
reported material and lightmap uploads.

## Safety and placement

- Display server: KDE Plasma Wayland with Xwayland display `:1`.
- Hardware renderer: AMD Radeon Graphics (Renoir), Mesa radeonsi, direct
  rendering enabled.
- DP-1 at `0,0 1920x1080` contained the running Kingdom Rush game and was never
  used for an engine window.
- eDP-1 geometry was `892,1080 1920x1080` at 164.83 Hz.
- Every final validation launch set `GENESIS3D_NO_INPUT_CAPTURE=1`,
  `GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080`.
- Observed engine client geometry was `892,1108 1920x1052`, wholly within
  eDP-1 after KWin decorations.
- The unattended X11 `InputHint=False` addition kept both keyboard focus and
  `_NET_ACTIVE_WINDOW` on window `85983241`; the engine window was `94371842`.
- All engine processes were stopped after testing.

## Source changes made for validation

`linux_main.cpp` now:

1. Accepts `GENESIS3D_WINDOW_X/Y`, applies `USPosition` before mapping, and
   remains normally movable after creation.
2. Waits for `MapNotify` before an enabled `XSetInputFocus`, avoiding the
   KWin/Xwayland `BadMatch` race seen when only `XSync` was used.
3. Implements `GENESIS3D_NO_INPUT_CAPTURE=1`, skipping explicit X11/SDL focus,
   grab, and relative-mouse requests.
4. Sets X11 `InputHint=False` in unattended mode so KWin does not automatically
   focus the mapped test window.
5. Reports render-deadline misses, maximum lateness, schedule resynchronization,
   elapsed-time clamps, and maximum fixed simulation steps per frame.

`README.md` documents unattended mode and initial placement overrides.

## Clean physical-GPU cadence

All runs loaded the local, non-distributed `Assets/Maps/rfedit1.bsp` fixture and
reported 6,436 root faces, 97 materials, and 7,915 lightmaps.

| Target | Frames | Seconds | Achieved FPS | Resyncs | Deadline misses | Max lateness | Elapsed clamps | Max steps/frame |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 60 | 578 | 9.657 | 59.86 | 1 | 10 | 23.197 ms | 0 | 2 |
| 120 | 757 | 9.642 | 78.51 | 319 | 757 | 31.026 ms | 0 | 4 |

Interpretation: 60 FPS passes. 120 FPS fails on the current renderer. The
regulator uses `std::chrono::steady_clock`, an absolute `frame_deadline`, and
`sleep_until(frame_deadline)`. A late schedule is advanced or resynchronized
to a future absolute deadline; it never performs a presentation catch-up loop.

## Bounded accumulator stress

A one-second `SIGSTOP`, followed by `SIGCONT`, simulated a severe scheduling
stall without consuming resources needed by the user's running game.

| Fixed rate | Frames / seconds | Elapsed clamps | Max fixed steps/frame | Expected 250 ms maximum |
|---|---:|---:|---:|---:|
| 60 | 277 / 5.637 | 1 | 15 | 15 |
| 120 | 360 / 5.629 | 1 | 30 | 30 |

The observed maxima exactly equal `0.25 / fixed_dt`. Catch-up is finite and
cannot grow with a stall longer than 250 ms.

## Camera movement review

The camera basis is computed once per frame and the same `CameraBasis` is used
for movement and rendering. `planar_forward=(cos(yaw),0,sin(yaw))` and
`right=(-sin(yaw),0,cos(yaw))` are unit-length and orthogonal. Forward and
strafe contributions are summed and passed through `normalized()`, so W+D has
the same translation magnitude as W alone. Each fixed update translates by
`move_speed * sprint * fixed_dt`.

This is a structural/mathematical validation only. Responsiveness and control
feel were not tested because all unattended launches disabled input capture.

## Visual result

The compositor-backed eDP-1 capture at
`/tmp/genesis3d-visual-safe.QdVpB1/edp1.png` shows a stable outlined BSP volume,
but its entire surface is nearly black against the dark clear color. No tears,
random corruption, or edge flicker are evident in the single capture. However,
the reported material/lightmap uploads are not visibly represented, so the
visual-equivalence gate fails. There is no original-renderer reference image in
the repository, preventing pixel-difference comparison.

## Signal teardown and memory

- Production SIGINT run: status 0 after a render summary.
- Production SIGTERM run: status 0 after a render summary.
- Clang ASan+UBSan SIGINT and SIGTERM runs both reached normal teardown without
  an address or undefined-behavior report.
- LeakSanitizer reported an identical 13,395 bytes in 130 allocations for both
  signals. Stacks originate in `glXChooseVisual` and SDL/driver shutdown; none
  enters `VFile/` or an engine allocator.
- Hardware Valgrind teardown reported 160 directly lost and 13,104 indirectly
  lost bytes, again in SDL/driver shutdown stacks, with no VFile stack.

Conclusion: the `VFileRegistryGuard` runs on both normal signal exits and has no
observed VFile-owned loss. The live SDL/GLX compatibility stack has external
shutdown allocations, so the process-wide zero-leak gate is not met.

## Remaining release gates

1. Diagnose the black material/lightmap result and obtain a reference capture
   from the original renderer for real equivalence testing.
2. Profile/rework the display-list immediate-mode submission path to reach 120
   FPS, or revise the supported target based on hardware requirements.
3. Perform user-driven physical WASD/mouse feel testing; unattended automation
   intentionally cannot establish subjective responsiveness.
4. If process-wide leak cleanliness is required, add targeted suppressions for
   proven library globals or investigate SDL2-compat/GLX shutdown ownership.

## Reproduction environment prefix

After this validation, unattended behavior was made fail-closed in the binary:
input capture is disabled unless `GENESIS3D_ENABLE_INPUT_CAPTURE=1` is set, and
the default window origin is eDP-1 at `892,1080`. Agent runs must never enable
interactive capture. They should still include this explicit defense-in-depth
override:

```sh
GENESIS3D_NO_INPUT_CAPTURE=1 \
GENESIS3D_WINDOW_X=892 \
GENESIS3D_WINDOW_Y=1080
```

Do not omit this prefix while another game is active.
