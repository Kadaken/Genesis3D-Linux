# Genesis3D Render Release Gates

Date: 2026-09-03

## Result

The BSP visual gate and 120 FPS cadence gate now pass on the native AMD Renoir
GPU. The shutdown audit found and corrected a real GLX/SDL teardown-order bug;
the remaining leak records are library-owned globals. Narrow suppression files
are supplied, but the Mesa LeakSanitizer record cannot safely be suppressed on
this system because its unloaded driver frames are no longer symbolized.

## Visual diagnosis and correction

The reported uploads were real. Runtime instrumentation measured an average
material component value of 90.2/255 and an average lightmap component value of
41.2/255. The black capture was therefore not caused by empty texture data.

The decisive fault was the initial view: it was placed outside the root BSP
bounds and aimed at the center of a sealed level. Genesis3D levels are authored
to be viewed from inside, and their exterior hull is black. The renderer now
uses the map's `PlayerSetup` entity origin (`-447 -46 232` in `rfedit1.bsp`) and
falls back to the bounds-derived view only when that entity is unavailable.

The OpenGL texture path was also corrected and hardened:

- Palettized `geBitmap` data is expanded through its palette instead of being
  passed to a non-palettized pixel decomposer.
- `GL_UNPACK_ALIGNMENT` is set to 1 for tightly packed RGB lightmaps whose row
  widths are not necessarily four-byte aligned.
- Material and lightmap UVs use the world surface shifts, draw scales, lightmap
  minima, and the legacy half-texel (`+8`) lightmap convention.
- Fixed-function lighting is explicitly disabled, vertex color is white, and
  texture environment mode is `GL_MODULATE` on both texture units.
- A one-pixel white lightmap is bound for faces without lightmaps so the same
  single-pass state is valid for every face.

The eDP-1 capture at `/tmp/genesis-playerstart.1unXyf/window.png` visibly shows
the level interior with checkerboard flooring, green wall panels, ceiling and
wall materials, colored emissive panels, and per-surface light variation. This
passes the prior “almost entirely black” gate. Pixel equivalence against the
original Windows renderer remains unmeasurable because the repository contains
no reference capture.

## Render bottleneck and 120 FPS result

The prior display list encoded two complete immediate-mode polygon passes per
lit face. It rebound the material for pass one, rebound the per-face lightmap
for pass two, changed blend/depth state, and resubmitted all vertices. With
7,915 lightmaps this produced thousands of extra binds and duplicated geometry
work every frame, even though the CPU-side list construction itself was cached.

The revised list uses two texture units and submits each face once. Unit 0
samples the material and unit 1 samples the lightmap; fixed-function modulation
performs the same multiplication as the old `GL_DST_COLOR, GL_ZERO` second
pass. The immutable list is compiled and completed in an explicit warmup frame
before cadence accounting, so a one-time startup cost is not mislabeled as a
recurring render-deadline failure.

Hardware measurements at 1920x1052 client resolution:

| Path | Frames / seconds | Achieved FPS | Schedule resyncs | Render deadline misses | Maximum lateness |
|---|---:|---:|---:|---:|---:|
| Prior two-pass display list | 502 / 6.525 | 76.94 | 225 | 502 | 31.919 ms |
| One-pass, capture run | 420 / 3.510 | 119.66 | 1 | 4 | 9.729 ms |
| Final clean run | 1,260 / 10.500 | **120.00** | **0** | 108 | 3.636 ms |

The 120 FPS throughput target passes and absolute schedule resynchronizations
fell to zero. There were 108 small render-finish deadline misses in the final
run, so the stronger zero-jitter criterion does not pass. The maximum miss was
3.636 ms and did not reduce achieved cadence. Eliminating all compositor and
scheduler jitter would require a different presentation contract, not more BSP
submission optimization.

## SDL/GLX shutdown allocations

The earlier Valgrind trace exposed an engine-owned lifecycle error: `SDL_Quit`
released process-wide GLX display state before the engine called
`glXDestroyContext`, producing invalid reads/writes in Mesa. Shutdown now runs
in this order:

1. Destroy the SDL wrapper for the foreign X11 window.
2. Delete display lists and textures while the GL context is current.
3. release current context and call `glXDestroyContext`.
4. Call `SDL_Quit`.
5. Destroy the X11 window/colormap, free the visual, and close the display.

After that correction, Clang ASan+UBSan completed the signal-driven run with no
address or undefined-behavior diagnostic. LeakSanitizer changed from 13,395
bytes in 130 allocations to 13,316 bytes in 129 allocations. The remaining
records divide into:

- 13,188 bytes rooted in `/lib64/libSDL2-2.0.so.0` during `SDL_Quit_REAL`; the
  Fedora library is an SDL2 compatibility ABI over SDL3, and these are its
  environment/hash/driver shutdown globals.
- 128 bytes initialized below `glXChooseVisual`; the DRI module is unloaded
  before LeakSanitizer reports it, so those frames appear anonymous.

No remaining allocation stack enters `VFile/`, `Bitmap/`, `World/`, another
engine allocator, or the new renderer data structures. The previously observed
GLX use-after-free was fixed rather than suppressed.

`tools/sanitizers/genesis3d-lsan.supp` narrowly matches only the SDL shared
library object. It deliberately does not suppress the anonymous 128-byte Mesa
record because `leak:main` would hide genuine engine leaks.
`tools/sanitizers/genesis3d-valgrind.supp` scopes records to
`SDL_Quit_REAL` or `glXChooseVisual`. These suppressions are suitable for a
library-clean CI view, while an unsuppressed sanitizer run remains the required
check for regressions in engine-owned memory.

## Verification and safety

- Production CMake build completed at 100%.
- The existing Clang ASan+UBSan build completed at 100%; its pre-existing
  Clang enum-conversion warnings remain outside this render-gate scope.
- Final sanitizer runtime: 382 frames / 6.367 seconds at 60.00 FPS, zero
  resyncs, zero render deadline misses, and no ASan/UBSan fault before the
  external library leak report.
- Every completed engine measurement used
  `GENESIS3D_NO_INPUT_CAPTURE=1`, `GENESIS3D_WINDOW_X=892`, and
  `GENESIS3D_WINDOW_Y=1080`.
- Verified engine client geometry was `892,1108 1920x1052`, wholly on eDP-1.
- Kingdom Rush remained at `0,0 1920x1080` on DP-1 during the checks.
- No Genesis3D, sanitizer, debugger, or Valgrind process was left running.

Safety incident: one early batch-debugger diagnostic accidentally omitted the
mandatory placement variables. It was detected and terminated immediately,
but its placement could not be proven after termination. The launcher was then
made fail-closed: input capture now requires the explicit user-only
`GENESIS3D_ENABLE_INPUT_CAPTURE=1`, `GENESIS3D_NO_INPUT_CAPTURE` always wins,
and the default window origin is eDP-1 (`892,1080`). No agent test sets the
interactive opt-in.

## Remaining limitations

- True visual equivalence still needs a reference capture from the original
  renderer at the same map location, view transform, resolution, and gamma.
- The native bridge renders the immutable root-face stream, not the original
  engine's full visibility, dynamic-light, animated-surface, actor, and
  translucent-sorting pipeline.
- Process-wide unsuppressed zero-leak output is not achievable with the current
  Fedora SDL2-compat/Mesa stack. Engine-owned zero-leak cleanliness is supported
  by stack ownership evidence; library-clean output requires the narrow
  suppressions above.
