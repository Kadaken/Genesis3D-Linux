# Description of modifications

This file documents changes to Covered Code as required by section 3.4 of the
Genesis3D Public License v1.01.

## 2026-09-10 — legacy BSP texture layouts and transparency

- Replaced the inherited one-byte format guess with bounds-checked payload-span
  detection. Genesis3D 1.1 raw indexed mip chains, later raw RGB/ARGB chains,
  and the subsequent format-prefixed variants are distinguished without ever
  consuming the first texel as metadata.
- Indexed payloads remain indexed until the bitmap conversion layer applies
  their per-texture palette. Indexed transparent faces resolve the historical
  key colour to its palette index before conversion to the driver's alpha
  format. Raw RGB colour keys retain their existing path. Unknown layouts fail
  with a diagnostic.
- Added synthetic lightmapped GBSP coverage for asymmetric indexed palette
  entries, a keyed indexed surface, an asymmetric raw-RGB surface, and a keyed
  raw-RGB surface. The cases discriminate byte alignment, palette order,
  conversion, OpenGL upload and key transparency.

Affected inherited file: `World/WBitmap.c`.

## 2026-09-10 — fixed-width BODY writer and actor pose gate

- `Actor/body.c` now writes vertices, normals, bones, materials, and triangles
  through the same explicit fixed-width BODY 00F1 records used by the reader.
  This restores writer/reader symmetry on LP64 hosts; notably, a native
  eight-byte bitmap pointer is never serialized in place of the historical
  four-byte bitmap-presence token.
- `tests/actor_pose_chain_test.c` writes and reloads a generated three-bone ACT,
  checks decoded keyframes and rest/animated transforms, and verifies three
  skinned vertex positions against hand-computed results.

## 2026-09-10 — authored entity defaults in application schemas

- `Entities/Entities.c` now retains each `%defaultvalue%` string from an
  authored `%typedef%` instead of asserting its position and discarding it.
- `Entities/EntitySchema.c` copies that metadata when replacing an authored
  layout with an application-declared native layout. Before instance epairs are
  parsed, omitted fields receive their authored default through the same
  checked application-type conversion path. Explicit instance values,
  including zero, are parsed afterwards and therefore always win.
- Missing or unparseable defaults remain zero and use the existing once-per-
  field diagnostic bit. Field/class destruction owns and releases copied
  default strings.
- The synthetic schema fixture distinguishes omitted 255 from explicit zero
  and covers an unparseable default's zero fallback and single diagnostic.

Affected files: `Entities/Entities.c`, `Entities/ENTITIES.H`,
`Entities/EntitySchema.c`, `Genesis.h`, and `tests/entity_schema_test.c`.

## 2026-09-09 — bounded world-face render scratch

- World/World.h and World.c: raise render scratch from 64 to 128 vertices.
  A measured 75-vertex BSP face exceeded the inherited Windows arrays too;
  disabling its assertion in Release did not make those writes safe.
- RenderFace budgets input vertices plus the active frustum mask's clip-plane
  count before writing. Overflow is skipped with one retained diagnostic per
  face per world lifetime. High-count clip passes also count their output before
  writing, since wave deformation need not preserve convexity.
- Sky setup respects both the 128-vertex scratch and 10-vertex output arrays;
  sky rendering respects its separate 30-vertex scratch. Both derive clipping
  counts from their existing loops (including the existing far-plane omission).
  Sky-portal frustum construction failure is now checked before use.
- Synthetic hidden-frame fixture covers 63/64/75/130 vertices, the five-plane
  budget boundary at 123/124, a clipped 75-vertex polygon, flat-color readback,
  and repeated-frame diagnostic counts. No map rewriting or format change.

## 2026-09-09 — defined 32-bit pixel composition

- Bitmap/pixelformat.c: six RGBX/BGRX/RGBA/ARGB/BGRA/ABGR composers now
  convert channels to uint32 before shifting and combine with bitwise OR.
  Signatures, channel order, assertions and ignored-alpha semantics unchanged.
- Public-API fixture covers all combinations of 0/127/128/182/255 in four
  input channels for each format (3,750 cases). It checks independent packed
  word order plus round trips; RGBX/BGRX still decode alpha as 255.
- Each original composer independently triggers UBSan on top-byte value 128.
  This is an in-range arithmetic fix, not a malformed-input handling change.

## 2026-09-09 — rejected actor cleanup

- Actor/actor.c: clear closed Header/Body subfile handles and route missing
  Motions through existing failure cleanup. Preserve NULL and the file-read
  diagnostic; no archive acceptance rules change.
- Added a generated public-VFS actor rejection fixture: valid empty-body
  control, missing Motions, missing required Body, and header-only rejection.
  Error-code interception forwards to the real logger; runtime logging is
  unchanged. This is not a malformed-actor parser or nonempty writer audit.

## 2026-09-08 — application-declared entity layouts and borrowed names

- Added copied class/field declarations and an explicit schema-aware world
  constructor. Original world construction remains available without overrides.
- Native-aligned field layout replaces the authored layout before allocation;
  text values resolve by field name. Added scalar-to-Y-vector and integer-to-float
  conversion, zero initialization, removal/invalid-value diagnostics, and retained
  name-based entity links when declared as opaque pointers.
- Added borrowed immutable entity names with world/entity lifetime.
- Added synthetic world fixtures for conversion, inserted fields, native offsets,
  retained links, copied declarations, failed construction cleanup, legacy-constructor
  isolation and name lifetime. No application schemas or assets are included.
- Changed `Entities/Entities.c`, `Entities/ENTITIES.H`, `Genesis.h`, `World/World.c`;
  added `Entities/EntitySchema.c` and test/build support.

See `Reports/ENTITY_SCHEMA_CONTRACT.md` for the API contract and test limits.

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

## 2026-09-10 — Application declaration defaults

- Extended the generic entity declaration record with an optional copied
  default string. World construction applies values in this order: instance
  epair, level `%defaultvalue%`, application declaration default, then zero.
- Declaration-only fields now parse instance/default text using their declared
  type instead of being left zero unconditionally. Boolean declaration text
  accepts `True` and `False` in addition to numeric values.
- Expanded the synthetic schema fixture across declaration-only, level-wins,
  explicit-zero, invalid-default, and caller-lifetime cases.

## 2026-09-10 — Legacy-driver flat-colour gate

- Added a lighting-free legacy engine/driver fixture that submits independent
  24-bit RGB textures at 255/0/0, 0/255/0, 0/0/255, and 128/128/128.
- The test asserts exact framebuffer readback for every channel, separating
  pixel-format and palette handling from BSP lighting, gamma, and content.
- No renderer behavior changed.

## 2026-09-10 — Keyed-texture mip edge quality

- The inherited OpenGL driver now propagates the nearest opaque edge RGB into
  alpha-zero texels before GLU builds mip levels. Alpha remains byte-for-byte
  unchanged and the engine-owned source bitmap is never modified.
- Added an asymmetric keyed-edge regression that reduces the prepared texture
  to a one-pixel sample and rejects darkening from zero-RGB transparent texels.
- This is a deliberate OpenGL quality improvement. The historical application
  used its Direct3D driver, so this change is not claimed as original-renderer
  fidelity restoration.

## 2026-09-11 — BODY F2 smooth skinning and GLB conversion

- Added the explicitly versioned BODY `0x00F2` disk format with little-endian
  fixed-width records, four normalized influences, inverse-bind transforms,
  and a 256-bone limit. The inherited `0x00F1` reader/writer/runtime path is
  unchanged and has a byte-identity control.
- Added CPU weighted deformation for positions, normals, and tangent-space
  lighting, with strict influence validation and a measured rigid fast path.
- Added the standalone `gltf2act` tool using pinned MIT `cgltf` and system
  libpng. Output is reopened and validated before atomic installation.
- Added asset-free numeric, malformed-input, round-trip, deterministic GLB,
  embedded-PNG, material, skeleton, and motion fixtures. This does not claim an
  authored-content visual improvement yet.
