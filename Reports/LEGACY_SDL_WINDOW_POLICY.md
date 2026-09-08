# Legacy SDL window policy

Date: 2026-09-07. Status: TESTED, not physical multi-monitor verification.

The inherited driver now defaults to `SDL_WINDOW_FULLSCREEN_DESKTOP` when
given positive fullscreen dimensions. Only an explicit
`DRV_HOOK_EXCLUSIVE_FULLSCREEN` hook flag enables `SDL_SetWindowDisplayMode`
and exclusive `SDL_WINDOW_FULLSCREEN`. The standard `geEngine` initializer
sets flags to zero. Environment variables do not opt into exclusive mode.

`(-1,-1)` selects windowed mode. Exactly one `-1`, or other nonpositive
dimensions, fails before window state changes. The driver restores the prior
fullscreen state and, if changed, the prior requested display mode at teardown.
Restoration failures produce diagnostics.

`DRV_DriverHook` now has a flags field, so the driver ABI advances from 100.3
to **100.4**. Rebuild engine and driver together. Existing version checks reject
older driver modules before initialization. The separate native renderer API
is unchanged. `OGLDrv.h` documents single-context/non-reentrant ownership.

Files changed: `Engine/Drivers/Dcommon.h`, `Engine/engine.c`,
`Engine/Drivers/OpenGl/SDLWindow.c`, `Engine/Drivers/OpenGl/OGLDrv.h`, CMake,
and neutral fixture tests. Neither `Render.c` nor `THandle.c` changed.

## Evidence

The new `genesis3d_sdl_window_contract` test runs only under Xvfb. It checks:

- Both mixed-sentinel cases reject initialization.
- Windowed initialization succeeds; a second simultaneous initialization fails.
- Default fullscreen uses desktop flags without changing desktop dimensions.
- Explicit exclusive initialization uses exclusive flags.
- Cleanup restores windowed state and desktop dimensions.

The public suite passes **5/5** in GCC Release and Clang ASan/UBSan Debug,
with leak detection enabled and no suppressions. The two inherited frame
fixtures still produce their expected textured/lightmapped framebuffer values.
`libOglDrv.so` still exports only `DriverHook`.

Reproduce from the root with `cmake --build build --clean-first -j2` and
`ctest --test-dir build --output-on-failure`. Sanitizer configuration follows
the initial driver report; run with `ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

Not covered: physical KWin/Wayland fullscreen transitions, refresh-rate
selection, multiple monitors, hardware cadence, context concurrency, or all
historical rendering features. The exclusive-mode test uses a virtual display,
not the physical desktop. These are policy/lifecycle tests, not GPU benchmarks.
