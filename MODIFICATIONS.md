# Description of modifications

This file documents changes to Covered Code as required by section 3.4 of the
Genesis3D Public License v1.01.

**Prominent derivation statement:** This work is derived directly and
indirectly from Genesis3D Original Code provided by Eclipse Entertainment,
later Genesis3D 1.1 material carrying WildTangent notices, and subsequent
community modifications preserved by Reality Factory. Kadaken does not claim
authorship or ownership of the original engine.

## 2026-09-03 — Kadaken native Linux port

- Added CMake discovery and compilation for the active portable engine graph.
- Added a native x86-64 Linux runtime frontend using SDL2, X11/XWayland, and
  OpenGL.
- Added BSP world ingestion, cached visible geometry, materials, baked and
  runtime lightmaps, translucent ordering, and actor render submission.
- Added ACT/BODY LP64 disk-record parsing, skeletal pose advancement, and
  animation playback.
- Added SDL keyboard/mouse handling and fixed-step camera movement.
- Added swept-hull BSP player collision, stairs/slopes, gravity, floor
  snapping, jumping, wall sliding, and ceiling rejection.
- Added monotonic 60/120 FPS pacing, bounded catch-up, signal-safe shutdown
  requests, and engine-owned resource teardown.
- Replaced or isolated active Win32 dependencies with standard C/POSIX types,
  filesystem operations, timing, and synchronization.
- Replaced active MSVC-only assembly/runtime constructs with portable C and
  explicit-width types.
- Corrected case-sensitive include spellings required on Linux.
- Added asset-isolation and fixed-width string-patching tools; no third-party
  game assets are distributed.
- Added sanitizer suppressions narrowly scoped to documented external library
  globals and verification reports describing test boundaries.
- Removed unused third-party SDK/header copies with restrictive notices from
  the current native branch; see `LEGAL.md`.

The commit history contains a more granular record. Verification results and
known gaps are in `Reports/VERIFICATION_SCOPE.md`.

## 2026-09-04 — Native shared render API

- Added the versioned `Engine/LinuxRender.h` C ABI and opaque renderer runtime.
- Added `libgenesis3d_render.so.1` as a reusable CMake shared-library target.
- Exposed exact-map loading, generic entity queries, SDL input state, BSP
  player stepping, application-owned camera submission, simulation advancement,
  and single-frame presentation.
- Preserved `genesis3d_linux` as a standalone executable linked against the
  same shared engine implementation.
- Added a headless API mode for world/entity/collision tests without creating a
  window or capturing desktop input.

## 2026-09-05 — BSP submodels and authored motion

- Extended the renderer command stream to submit non-root BSP models with
  their runtime transforms, materials, lightmaps, and translucent ordering.
- Added bounded C API queries for world-model count, lookup, bounds, entity
  model references, authored motion extents, and motion-time sampling.
- Routed authored motion samples through `geWorld_SetModelXForm` so drawing,
  traces, and collision observe the same transform and updated bounds.
- Added cumulative frame diagnostics for root-visible faces, submodel face
  submissions, and actor primitives.
- Added application-controlled submodel render visibility so private gameplay
  code can suppress logical brush volumes without removing their query data.
- Added bounded actor motion enumeration/selection and per-instance scale
  controls for application-owned animation state machines.
- Added bounded material name/dimension inspection and stride-aware RGBA texture
  replacement so application layers can implement animated surfaces without
  importing game-specific decoders or assets into the engine.
- Added bounded player gravity, jump-speed, and step-height configuration so an
  application can apply authored map movement without renderer coupling.
- Added version-19 generic actor Euler transforms and a view-model render flag.
  View actors render after the world with an isolated depth buffer while asset
  choice, offsets, animation policy, and weapon behavior remain application-owned.
- Completed actor scale propagation through both pose attachment/motion
  translations and body vertices, preventing scaled skeletal pieces from
  separating.
- Added binary-alpha rejection for actor materials and a dedicated close-range
  view projection.
- Made framebuffer capture redraw into the owned back buffer before readback,
  avoiding driver-dependent post-swap buffer retention.
- Added the version-20 protected world-space callback. Applications can draw
  transient world presentation after BSP geometry while the renderer brackets
  the callback with complete OpenGL attribute and matrix restoration.
- Enabled negotiated 4x multisample anti-aliasing in both the reusable runtime
  and standalone harness, with a non-MSAA window-format fallback.
