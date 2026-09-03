# Genesis3D Native Linux PQPW Architectural Audit

Date: 2026-09-03

## Scope and result

The audit covered every compilation unit in the active `genesis3d_linux`
target, with focused refactoring in the native bootstrap, system gateway, BSP
loader, entity layout, actor data, bitmap conversion, world traversal, and
virtual filesystem lifecycle.

The resulting target completes a clean GCC build at 100%, produces zero GCC
compiler warnings, produces zero GCC `-fanalyzer` findings, loads the real
`rfedit1.bsp`, and completes an off-screen Clang AddressSanitizer plus
UndefinedBehaviorSanitizer render/shutdown run without an address or undefined
behavior error. A headless Valgrind run reports zero errors and zero definite,
indirect, or possible leaks owned by the engine.

Physical-GPU cadence and visual validation were subsequently performed on the
built-in eDP-1 panel. The measured 60 FPS target passes, while the 120 FPS and
visual-equivalence gates currently fail. Details are recorded under
"Hardware validation" below.

## Refactored files

### Build policy

- `CMakeLists.txt`
  - Keeps SDL2 as an explicit imported dependency.
  - Makes pointer-to-integer truncation, integer-to-pointer conversion,
    implicit declarations, and incompatible pointer arguments fatal for every
    active C compilation unit.

### Native render, input, and pacing

- `linux_main.cpp`
  - Shares one camera basis across movement and rendering, calculating each
    pitch/yaw trigonometric term once per frame.
  - Normalizes diagonal movement and retains fixed-step camera translation.
  - Compiles the immutable BSP texture/lightmap fan stream once into a driver
    display list rather than repeating face lookup, UV calculation, and command
    construction every frame.
  - Caches blend, depth-write, depth-function, and bound-texture state while the
    static command stream is constructed, omitting duplicate state calls.
  - Replaces render-time-plus-sleep drift with monotonic absolute deadlines.
  - Uses a bounded 250 ms fixed-step accumulator to prevent a long scheduling
    pause from causing an unbounded catch-up loop.
  - Adds frame count, achieved FPS, and missed-deadline diagnostics.
  - Handles SIGINT/SIGTERM through a signal-safe stop flag and normal teardown.
  - Adds an RAII VFile registry guard.

### 64-bit and memory correctness

- `Actor/body.c`
  - Replaces a bitmap pointer-to-`uint32` sentinel comparison with `uintptr_t`.
  - Initializes the body pointer before all error branches and hardens partial
    destruction against null input.
- `Bitmap/bitmap_blitdata.c`
  - Converts four legacy 32-bit pointer-alignment checks to `uintptr_t`.
- `Entities/ENTITIES.H`, `Entities/Entities.c`
  - Records native alignment for every entity field type.
  - Aligns each generated field offset before allocation, preventing unaligned
    8-byte pointer stores in layouts originating from 32-bit map definitions.
  - Validates size/alignment and handles failed class creation without relying
    on debug-only assertions.
- `World/World.h`, `World/World.c`
  - Changes the geometry index API from platform-variable `long` to explicit
    `int32`.
  - Adds triangle-count overflow and allocation-failure handling.
  - Corrects `*Count++` to `(*Count)++` in recursive area traversal.
  - Initializes fog cleanup state before error branches.
- `Actor/motion.c`
  - Initializes mixed rotation/translation samples before recursive sampling.
- `Bitmap/Compression/paloptimize.c`
  - Validates palette inputs and initializes the rollback palette before use.
- `Actor/strblock.c`, `Bitmap/bitmap_gamma.c`, `Actor/path.c`
  - Removes analyzer-confusing nullable/no-op expressions and redundant local
    `min`/`max` macro definitions.

### Native-only and lifecycle hygiene

- `Engine/System.c`
  - Removes Windows headers, timing branches, DLL names, `GetProcAddress`,
    `FreeLibrary`, Direct3D/Glide/Software/Wire driver references, and obsolete
    Glide-selection logic.
  - Retains only POSIX monotonic timing, `dlopen`/`dlsym`/`dlclose`, and the
    native OpenGL driver name.
- `World/Gbspfile.c`
  - Removes an unused system-header dependency and 27 disabled debug-print
    remnants while retaining the explicit 80-byte legacy disk record.
- `VFile/vfile.c`
  - Preserves API constness and removes the cast-away-const path.
  - Makes registry shutdown idempotent and resets all registry state.
  - Releases the built-in filesystem registry during normal application
    teardown, eliminating the engine-owned 56-byte Valgrind loss record.

## Defects found by independent analysis

The first GCC `-fanalyzer` pass produced 15 reports representing seven unique
root causes: uninitialized body cleanup, nullable string-block movement,
uninitialized motion samples, uninitialized palette rollback, unchecked entity
class allocation, uninitialized fog cleanup, and unchecked geometry allocation.
After correction, the final analyzer pass produced zero analyzer findings.

The first UBSan runtime stopped at `Entities/Entities.c` on an 8-byte pointer
store to a 4-byte-aligned address. Native per-field layout alignment fixed the
root cause. The subsequent ASan/UBSan run loaded 6,436 root faces, 97 material
textures, and 7,915 lightmaps, rendered, handled SIGINT, and exited with status
0 and no address/UB report.

The first detailed Valgrind pass found no invalid memory access and no definite
leak, but identified a 56-byte VFile registry as possibly lost. After the
explicit registry lifecycle repair, Valgrind reported:

```text
definitely lost: 0 bytes in 0 blocks
indirectly lost: 0 bytes in 0 blocks
possibly lost: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors from 0 contexts
```

The remaining 11,340 reachable bytes are loader/SDL compatibility globals with
live roots at process exit; they are not lost allocations.

## Performance evidence and limit

The regulator performs no spin waiting. It uses `steady_clock`, one bounded
simulation accumulator, and `sleep_until(frame_deadline)`. When a frame is
late, it advances to a future absolute deadline instead of busy-catching up.
This makes the idle path sleep-efficient at both configured rates.

The earlier off-screen Xvfb stress test used CPU-based Mesa rasterization at
1920x1080. It achieved 10.99 FPS for the 60 FPS request and 10.77 FPS for the
120 FPS request, consuming about 158% CPU and marking every frame late. Those
numbers measure software rasterization saturation only.

## Hardware validation

Hardware testing used direct GLX rendering on AMD Radeon Graphics (Renoir,
Mesa radeonsi) on the built-in eDP-1 panel. Every unattended run set
`GENESIS3D_NO_INPUT_CAPTURE=1` and created the window at the panel origin with
`GENESIS3D_WINDOW_X=892` and `GENESIS3D_WINDOW_Y=1080`. KWin placed the client
at 892,1108 with a 1920x1052 client area, wholly inside eDP-1. X11 input focus
and `_NET_ACTIVE_WINDOW` remained on the pre-existing window throughout.

Clean cadence measurements against `rfedit1.bsp` were:

| Target | Frames / seconds | Achieved | Schedule resyncs | Render deadline misses | Maximum lateness |
|---|---:|---:|---:|---:|---:|
| 60 FPS | 578 / 9.657 | 59.86 FPS | 1 | 10 | 23.197 ms |
| 120 FPS | 757 / 9.642 | 78.51 FPS | 319 | 757 | 31.026 ms |

The 60 FPS target is sustained. The 120 FPS target is not sustained by the
current immediate-mode display-list path and remains a failed performance
gate. Absolute deadlines use `steady_clock` and `sleep_until`; late frames
advance or resynchronize the next absolute deadline instead of busy-spinning.

A controlled one-second `SIGSTOP`/`SIGCONT` scheduler stall exercised the
worst-case accumulator path without adding load to other desktop applications.
At 60 FPS it recorded one elapsed clamp and at most 15 fixed steps in one
frame; at 120 FPS it recorded one elapsed clamp and at most 30 fixed steps.
Those are exactly the 250 ms bounds, demonstrating that catch-up work remains
finite after a long stall.

A compositor capture showed stable geometry edges but an almost completely
black BSP surface even though startup reported 97 material textures and 7,915
lightmaps uploaded. With no original-renderer reference capture in the
repository, pixel equivalence cannot be computed; the visibly untextured/dark
result nevertheless fails the requested visual-equivalence gate.

SIGINT and SIGTERM production runs both reached normal teardown and exited
with status 0. AddressSanitizer/UndefinedBehaviorSanitizer runs reached normal
teardown without an address or undefined-behavior diagnostic. LeakSanitizer
reported the same 13,395-byte SDL/GLX driver allocation set for both signals;
no allocation stack entered `VFile/` or an engine allocator. A hardware
Valgrind teardown likewise found no VFile stack, but classified SDL/driver
shutdown allocations as 160 directly lost plus 13,104 indirectly lost bytes.
The VFile registry RAII guard therefore passes its scoped leak check, while a
blanket zero-process-leak claim for the live SDL/GLX stack is not supported.

## Measured footprint

The two explicitly targeted dead-code files changed as follows:

| File | Before | After | Removed |
|---|---:|---:|---:|
| `Engine/System.c` | 17,478 bytes | 16,271 bytes | 1,207 bytes |
| `World/Gbspfile.c` | 18,457 bytes | 17,450 bytes | 1,007 bytes |
| Total redundant source removed | | | 2,214 bytes |

`linux_main.cpp` grew because the prior drifting sleep loop and repeated render
submission were replaced by cached geometry, state tracking, diagnostics,
fixed-step accumulation, and deterministic teardown. The ELF remains unstripped
to preserve useful diagnostic symbols.

## Verification commands

```sh
make clean && make
cmake -S . -B <temporary-analyzer-build> -DCMAKE_C_FLAGS='-fanalyzer -fno-common' -DCMAKE_CXX_FLAGS='-fanalyzer -fno-common'
cmake --build <temporary-analyzer-build> -j4
CC=clang CXX=clang++ cmake -S . -B <temporary-sanitizer-build> -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build <temporary-sanitizer-build> -j4
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./genesis3d_linux
```

## PQPW status

Build, static analysis, native map ingestion, VFile registry cleanup, hardware
60 FPS cadence, bounded catch-up, and clean signal shutdown are proven. The
120 FPS target and visual-equivalence gate fail on the tested hardware. Input
feel remains unverified because unattended runs deliberately disable input
capture, and real laptop energy draw was not measured.

The repository already contained a large, mixed, uncommitted native-port work
tree before this audit. These changes were not committed because an isolated
audit commit would either omit required pre-existing port work or capture
unrelated edits without provenance. A curated baseline commit remains a release
provenance requirement before this can honestly be called a reproducible PQPW
release.
