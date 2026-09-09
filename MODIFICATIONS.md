# Description of modifications

This file documents changes to Covered Code as required by section 3.4 of the
Genesis3D Public License v1.01.

## 2026-09-08 — generic entity userdata reservation

- Added public class field-size query and zero-tail userdata reservation APIs.
- Kept schema FieldSize immutable; internal per-entity capacity tracks growth.
- Repair declared struct/raw pointer references across the complete owning
  set after relocation, including self references, before freeing old blocks.
- Preserve values and prior tail edits; stage all allocations before changes
  so failure leaves original storage intact. Views cannot request reservation.
- Added an in-memory generated entity fixture with a discriminating stale-
  reference negative control. Public Release and matched sanitized suites 9/9.
- This reserves storage only: it does not migrate reordered/type-changed fields.

See `Reports/ENTITY_USERDATA_RESERVE.md` for contract and limits.

## 2026-09-08 — DOS binary short-read parity

- Posix_Read now matches the inherited FSDos_Read contract: DOS short reads
  at EOF succeed when at least one byte is read; zero bytes fail.
- FSVFS_Read remains unchanged: an over-length member read fails without
  advancing the member position. These are intentionally different contracts.
- Added generated 100-byte DOS/VIRTUAL read tests covering an 88-byte request
  after consuming 13 bytes, exact 87-byte reads, Tell and true EOF. The DOS
  regression fails against the previous implementation.

See `Reports/VFILE_LINE_AND_WRITE_CONTRACT.md` for results and limits.

## 2026-09-08 — bounded VFile lines and const-safe archive writes

- Replaced the POSIX fgets pass-through with a bounded raw-read/scan routine
  folding CRLF, CR and LF to LF followed by NUL, seeking back unread bytes.
- Intentionally differ from the legacy Windows reader: final unterminated
  lines and buffer-filling fragments return TRUE with NUL-terminated content,
  instead of returning FALSE after consuming and losing those bytes.
- Archive line capacity now reserves room for NUL without excluding the final
  member byte. Invalid capacities and actual member EOF return FALSE without
  reading. Assert against the saved member length, not a length updated by I/O.
- Plain archive writes use the const input directly. Nonzero-mask writes XOR
  into bounded 4 KiB stack chunks without modifying the caller's storage.
- Added asset-free DOS/VIRTUAL line and const-write fixtures. Release and
  matched Clang ASan/UBSan public suites pass 7/7; no suppressions added.

See `Reports/VFILE_LINE_AND_WRITE_CONTRACT.md` for exact coverage and limits.

## 2026-09-07 — explicit SDL fullscreen policy

- Defaulted the inherited SDL driver to desktop fullscreen. Exclusive mode
  switching requires `DRV_HOOK_EXCLUSIVE_FULLSCREEN` in the new hook flags.
- Advanced the driver ABI to 100.4 and initialized engine-supplied flags to
  zero. Engine and driver must be rebuilt together; older modules are rejected.
- Reject mixed windowed/fullscreen dimension sentinels and invalid dimensions;
  restore the caller's prior fullscreen/display-mode state on cleanup.
- Documented single-context, non-reentrant driver ownership and added an Xvfb
  contract fixture for mode policy and teardown. Rendering/texture algorithms
  were not changed.
- Extracted the neutral fixture writer for reuse by consumer-side boot tests.

## 2026-09-07 — inherited OpenGL driver on SDL

- Added the separate `libOglDrv.so` target for the historical `DRV_Driver`
  interface; the existing native renderer and standalone executable remain.
- Added an SDL host replacing the Win32 host in the Linux build. The caller
  owns the SDL window; the driver owns and releases only its OpenGL context.
- Guarded Windows-only initialization, used system GL extension declarations
  and SDL procedure lookup, corrected conflicting declarations, and exported
  the driver entry point without exporting implementation globals.
- Preserved the inherited polygon and texture submission algorithms; release
  texture handles before deleting the context and bound error-string copying.
- Added asset-free legacy frame fixtures, including a generated BSP with a
  measured half-intensity lightmap and window/context ownership checks.
- Verified all four CTests in Release and with Clang ASan/UBSan and leak
  detection under Xvfb/Mesa llvmpipe. No sanitizer suppressions were used for
  this gate. This is not hardware or full application verification.

## 2026-09-06 — texture-name query completion

- Completed the inherited `geWorld_GetTextureName` path, which previously
  reported a hit without ever writing a texture name.
- Added bounds validation, null-extents support, nearest leaf-face selection,
  bounded-polygon rejection, and a two-surface neutral-world regression
  fixture.
- Added an exact collision-result query that restricts lookup to the collided
  model, hit-plane orientation, and bounded face; documented the required
  32-byte output capacity in the public header.

## 2026-09-06 — public math-constant exposure

- Exposed the complete reciprocal and degree/radian conversion constants from
  the historical support header through the canonical root `basetype.h` so
  external engine consumers receive the same public math vocabulary.
- Kept the two historical same-named headers structurally separate for now;
  their Windows calling/export convention differences belong to the planned
  cross-platform Windows target rather than this narrow Linux consumer fix.
- Exposed the already-portable `geEngine_UpdateWindow` declaration to native
  consumers instead of hiding it behind the Windows-header include guard.

## 2026-09-06 — public header macro hygiene

- Removed the generic `min` and `max` function-like macros from the public
  `basetype.h` compatibility header because they corrupt C++ standard-library
  declarations in downstream consumers.
- Added engine-prefixed `GE_MIN` and `GE_MAX` equivalents and migrated active
  engine call sites to those names.

## 2026-09-05 — portable sound API and ABI v22

- Implemented the historical `geSound_*` interface over OpenAL, including WAV
  ingestion through `geVFile`, looping, pitch, source pan/gain, master gain, and
  deterministic definition/source teardown.
- Restored the portable `Sound3d.c` calculations to the native shared library.
- Defined the unavailable DirectSound escape hatch to return null on Linux.
- Exposed the existing engine constructor and vector element helper on Linux,
  and added the small vector maximum/zero helpers.
- Added a neutral in-memory WAV lifecycle test using OpenAL Soft's null backend.
- Advanced the native renderer ABI from 21 to 22.

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
- Added trilinear mipmaps and bounded anisotropic filtering to world materials;
  dynamic material replacements now refresh all mip levels.
- Extended the version-21 input ABI with bounded mouse-wheel edge counts. The
  renderer preserves SDL's high-resolution wheel motion and exposes neutral
  next/previous actions while leaving inventory policy to applications.
