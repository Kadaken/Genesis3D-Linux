# Full Render Pipeline Port — First Integrated Milestone

Date: 2026-09-03

## Result

The native Linux renderer now builds a camera-dependent frame command stream
instead of replaying the complete root-face display list. The stream uses the
compiled BSP leaf/cluster PVS when it is valid, preserves the existing
material/lightmap multitexture path, submits opaque faces first, and submits
translucent faces back-to-front with depth writes disabled. Runtime counters
cover visible and PVS-culled faces, actor primitives, translucent submissions,
animated surfaces, and active dynamic lights.

The original actor data path has also been connected through
`geActor_DefCreateFromFile`, `geActor`, `gePose`, `geBodyInst`, and embedded
`geBitmap` materials. A narrow actor-to-native bridge exposes the current posed
joint transform array without transferring ownership. Actor primitives are
ready to submit as triangles, strips, or fans when parsing succeeds.

The purified actor does not yet render. This is an explicit compatibility
blocker: the `.act` outer VFS and nested body archive are now reached, but the
legacy body geometry reader loses alignment before the bone-name string block
and reports `ERR_STRBLOCK_FILE_PARSE` / `ERR_BODY_FILE_READ`. No synthetic
geometry or invented format interpretation was used.

## Legacy architecture and ownership map

| Pipeline | Legacy owner | Driver boundary | Native milestone |
|---|---|---|---|
| Static visibility | `World/Vis.c`, leaf/cluster/area frame marks | `World.c::RenderBSPFrontBack_r` | Camera leaf and compiled cluster PVS select frame faces; invalid PVS safely falls back to root faces |
| Opaque/translucent ordering | `World/World.c` geometry list and `World_TransPoly` | `RenderWorldPoly` | Separate opaque and translucent queues; translucent distance sort and no depth writes |
| Materials/lightmaps | `World/Surface.c`, `World/Light.c` | texture handles, `DRV_LInfo`, `SetupLightmap` | Existing two-unit material × lightmap path preserved |
| Animated light styles | `Surf_SurfInfo::NumLTypes`, `Light_LightInfo` | `SetupLightmap` callback | Actual surface ownership is detected and counted; lightmap regeneration remains pending |
| Dynamic lights | `Light_LightInfo::DynamicLights`, per-surface `DLights` bits | `SetupLightmap` callback | Active world lights are exposed and counted; per-face dynamic-light compositing remains pending |
| Actors | `Actor/actor.c`, `pose.c`, `bodyinst.c`, `puppet.c` | Gouraud/misc polygon callbacks | Definition/pose/body-instance/material submission path implemented; body payload alignment blocks runtime geometry |

## Binary compatibility work

The first actor load exposed LP64 widening in structures that are persisted in
the 32-bit VFS format. The VFS file header, directory header, directory entry
size/offset fields, and `geVFile_Time` fields now use explicit 32-bit on-disk
widths. This moves loading from failure at the outer `VF00` archive to the
nested body geometry stream. The remaining body mismatch must be resolved with
explicit disk records for legacy structs (particularly structures whose native
layout or pointer width differs) and fixture-level offset tests.

## Completed / pending

| Capability | State | Evidence / next dependency |
|---|---|---|
| BSP/PVS culling | Completed milestone | 60 FPS run: 2,239,470 visible and 637,422 culled face submissions across 447 measured frames |
| Opaque queue | Completed | Submitted before translucent queue with depth writes enabled |
| Translucent sorting | Completed milestone | 113,985 sorted submissions in the 60 FPS run; distance key currently uses a representative face vertex |
| Material/lightmap multitexturing | Preserved | 97 materials and 7,915 lightmaps uploaded; means remain 90.2 and 41.2 / 255 |
| Actor archive parsing | In progress | Outer and nested VFS readable after width fixes; body geometry alignment still fails |
| Posed actor geometry/material rendering | Code path complete, runtime blocked | Requires explicit 32-bit body disk records and a successful four-asset fixture parse |
| Skeletal motion playback | Pending | Requires actor parse, motion time extents, and fixed-step pose advancement |
| Animated surface updates | Pending | Hook/counter uses `NumLTypes`; implement styled lightmap regeneration from actual light layers |
| Dynamic-light compositing | Pending | Hook/counter uses `LightInfo`; implement surface marking and transient lightmap upload |
| Area portal visibility/frustum clipping | Pending | Current milestone uses cluster PVS; area connectivity and polygon clipping remain legacy-side dependencies |

## Measurements and verification

- Production CMake build completed at 100%.
- Clang ASan+UBSan build completed at 100%; pre-existing enum-conversion and
  pragma warnings remain.
- Safe 60 FPS hardware run: 447 frames / 7.450 seconds, 60.00 FPS, zero
  schedule resyncs, one 0.247 ms render deadline miss.
- Safe 120 FPS hardware run: 892 frames / 7.487 seconds, 119.15 FPS, six
  schedule resyncs, 186 render deadline misses, 9.875 ms maximum lateness.
  This is a measurable regression from the immutable full-world display list;
  the next performance step is per-face GPU command caching while retaining
  the PVS-selected and sorted frame queues.
- Every launch used `GENESIS3D_NO_INPUT_CAPTURE=1`,
  `GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080` on eDP-1. No launch
  enabled input capture, and no Genesis3D process remained running.

Commit: `fcea1d4` (report hash note added in the immediately amended commit).
