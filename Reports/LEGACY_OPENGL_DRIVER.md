# Inherited OpenGL driver: neutral runtime gate

Date: 2026-09-07. Contributor: Kadaken.

## Result

The historical engine frame pipeline now executes on Linux through the
inherited OpenGL driver. No changes to the core engine/world algorithms were
needed for the two neutral fixtures. The existing `geLinuxRender` frontend
remains separate and unchanged.

This establishes a bounded integration prerequisite, not complete engine or
application compatibility. Both graphics tests used Xvfb and Mesa llvmpipe
(`LLVM 22.1.8, 256 bits`), not the physical GPU.

## Implementation and ownership

- `CMakeLists.txt`: builds the inherited driver as `libOglDrv.so`, separately
  from `libgenesis3d_render.so.1`; preserves `genesis3d_linux`; registers tests.
- `Engine/Drivers/OpenGl/SDLWindow.c`: replaces the Win32 host in the Linux
  target, implements display-mode enumeration, context creation and swap.
- `OGLDrv.c` / `OGLDrv.h`: isolate Windows setup, use SDL GL procedure lookup
  and system extension headers, correct declaration conflicts, export
  `DriverHook`, release texture handles before the context, bound error copies.
- `OglMisc.c`, `Render.c`, `THandle.c`, `THandle.h`, `Win32.h`: remove unnecessary
  Windows includes on active Linux paths and align declarations. GL extension
  pointer variables are renamed to avoid collisions with system prototypes.
  Polygon and texture submission algorithms are retained.
- `tests/legacy_frame_test.c` and `tests/neutral_bsp_fixture.h`: public,
  generated-data runtime fixtures. No third-party assets are required.

The application must initialize SDL video, configure framebuffer attributes,
create an `SDL_WINDOW_OPENGL` window, and pass that **SDL_Window pointer** as
the opaque window handle to `geEngine_Create`. It must also pass the directory
containing `libOglDrv.so`. The driver borrows the window and owns its GL context.
Select the enumerated `(-1, -1)` window mode for the tested path. Free the
engine before destroying the SDL window or shutting down SDL.

The driver does not create a second window, show a hidden window, pump input,
or grab the mouse. One active legacy context is supported per process.
Simultaneous use with the separate native render API is not verified.

The inherited `Win32.c` is retained, but is not part of the Linux target.
In addition to context/swap duties it contains display enumeration, fullscreen
mode changes, and `D3D24.ini` color/depth preferences. The SDL host enumerates
SDL modes and accepts caller framebuffer attributes instead of reading that
Windows-specific preference file. Fullscreen and multi-display selection are
not verified by this gate; enumeration does not establish monitor ownership.

## What the fixtures assert

Both call `geEngine_Create`, enumerate driver/mode, call
`geEngine_SetDriverAndModeNoSplash`, attach a world, execute
`geEngine_BeginFrame` / `geEngine_RenderWorld` / `geEngine_EndFrame`, and free
the engine. They read a framebuffer pixel before swap, check for GL errors,
then check that context teardown leaves the caller window alive.

| Fixture | World | Expected and measured center RGB |
|---|---|---|
| `genesis3d_legacy_frame_fixture` | Public empty-world constructor plus textured world polygon | `(24, 220, 48)` |
| `genesis3d_legacy_bsp_fixture` | Generated version-15 BSP: one root face, texture and half-intensity baked lightmap | `(12, 110, 24)` |

The second assertion discriminates a missing lightmap modulation path from a
working one. Pixel tolerance is two channel values. These are single-frame
tests, not sustained pacing or broad scene-correctness tests.

## Reproduction

Install SDL2, OpenGL/GLU development packages, Xvfb and xauth as described in
the root README. From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first -j2
ctest --test-dir build --output-on-failure

cmake -S . -B build-rf8-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build-rf8-sanitize -j2
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ASAN_OPTIONS=detect_leaks=1 \
ctest --test-dir build-rf8-sanitize --output-on-failure
```

Final terminal results:

```text
Release:       100% tests passed, 0 tests failed out of 4 (6.25 sec)
ASan/UBSan:    100% tests passed, 0 tests failed out of 4 (6.52 sec)
nm -D:         DriverHook exported from libOglDrv.so
ldd -r:        dependencies resolved; no undefined-symbol diagnostics
```

Sanitizers instrument the core, driver and fixture. Leak detection was enabled
without suppressions; no ASan, UBSan or leak diagnostics were reported by this
run. This does not supersede documented external allocations from earlier,
different native-frontend/hardware tests.

Release module SHA-256:
`3e653ed2db625889d9e9bc6f962437a55e4f8126094b4abf364f2e4cf8da85c7`.
Local logs are retained under `Reports/evidence/legacy-driver-2026-09-07/`
(ignored, not published). Test commands and fixture sources are reproducible
without those logs.

## Limits and history

Hardware cadence, input feel, fullscreen, resizing, context recreation, actor
rendering, dynamic light changes, transparency ordering, large worlds and
complete applications have not been verified through this legacy driver.
The splash is bypassed for the neutral test; this is not a distribution
compliance test or a change to release requirements.

Earlier reports record the separate native bridge and its root-face limitation
(`RENDER_RELEASE_GATES.md`) and later native command-stream expansion
(`FULL_RENDER_PIPELINE_PORT.md`). The inspected reports and modification log
do not establish a prior successful legacy `geEngine` frame run or document a
core-pipeline defect as the reason that bridge was chosen. Absence of such a
record is not proof that no earlier experiment occurred.
