# Smooth skinning extension design

Date: 2026-09-11

Status: design only. No engine code or actor format was changed in this pass.

Round-24 regression receipt: the unchanged engine passes 18/18 Release tests
and 18/18 matched Clang ASan/UBSan tests, executed serially. This verifies that
writing the design did not alter the current rigid-skinning implementation; it
does not test the proposed extension.

## Decision

Smooth skinning is an extension to the BODY geometry format and CPU pose path,
not an engine rewrite. The existing actor, motion, puppet, material, and world
interfaces remain usable. A GLB converter is a new standalone tool and is the
largest part of the work.

The current implementation is rigid skinning. `geBody_XSkinVertex` stores one
`BoneIndex`, and `Actor/bodyinst.c:304-344` transforms each vertex with exactly
one bone matrix. Searches of `body.c`, `bodyinst.c`, `pose.c`, and `puppet.c`
find no weight or blend-weight representation. Adding bones alone cannot blend
a shoulder, elbow, or hip across a joint.

## 1. Versioned BODY format

The current Geometry member has magic `0x5E444F42`, version `0x00F1`, and the
reader rejects every other version at `Actor/body.c:1489`. Keep the type magic
and add version `0x00F2`. The reader must dispatch explicitly:

- `0x00F1`: execute today's reader and runtime layout unchanged. Every vertex
  behaves as one influence with weight 1.0, using today's bone-local point.
- `0x00F2`: read the fixed-width records below and enable weighted skinning.
- any other version: fail with the existing BODY parse diagnostic.

All F2 integers and IEEE-754 float bit patterns are little-endian and are read
field-by-field. The implementation must not serialize a native C structure or
depend on compiler packing.

```c
#define GE_BODY_F2_MAX_INFLUENCES 4

/* 4 bytes */
typedef struct geBody_DiskInfluenceF2 {
    uint16_t bone_index_le;
    uint16_t weight_unorm16_le;
} geBody_DiskInfluenceF2;

/* 40 bytes */
typedef struct geBody_DiskSkinVertexF2 {
    uint32_t point_x_f32_le;
    uint32_t point_y_f32_le;
    uint32_t point_z_f32_le;
    uint32_t u_f32_le;
    uint32_t v_f32_le;
    uint8_t lod_mask;
    uint8_t influence_count;
    uint16_t reserved_le;
    geBody_DiskInfluenceF2 influences[4];
} geBody_DiskSkinVertexF2;

/* 32 bytes */
typedef struct geBody_DiskNormalF2 {
    uint32_t normal_x_f32_le;
    uint32_t normal_y_f32_le;
    uint32_t normal_z_f32_le;
    uint8_t lod_mask;
    uint8_t influence_count;
    uint16_t reserved_le;
    geBody_DiskInfluenceF2 influences[4];
} geBody_DiskNormalF2;

/* 124 bytes: the F1 bone record plus an inverse bind matrix */
typedef struct geBody_DiskBoneF2 {
    uint32_t bbox_min_f32_le[3];
    uint32_t bbox_max_f32_le[3];
    uint32_t attachment_f32_le[12];
    int16_t parent_bone_index_le;
    uint16_t reserved_le;
    uint32_t inverse_bind_f32_le[12];
} geBody_DiskBoneF2;
```

F2 points and normals are in mesh bind space. Each bone carries the inverse of
its global bind transform. Runtime skin matrices are therefore
`current_global_bone * inverse_bind_bone` before camera and actor scale are
applied.

Validation is mandatory before allocation or use:

- influence count is 1–4;
- every bone index is below the declared bone count;
- unused influence slots are zero;
- weights sum exactly to 65,535 and at least one is nonzero;
- all decoded floats and matrices are finite;
- counts fit the current signed 16-bit runtime indices; and
- every multiplication used for allocation is checked for overflow.

The converter normalizes source weights in float, keeps the four greatest,
renormalizes, and quantizes with largest-remainder correction so the stored sum
is exactly 65,535. The loader validates; it does not silently repair malformed
files.

The in-memory `geBody` gains optional influence and inverse-bind arrays plus a
format/skinning mode. The F1 writer remains byte-compatible. The writer emits
F2 only when weighted metadata was deliberately attached through new body
builder APIs. This prevents a read/write cycle from silently upgrading an old
asset.

Normals need the same influences as positions. After weighted rotation they
are normalized. Tangents and bitangents used by the DOT3 path must use the same
blend and be orthonormalized against the blended normal; otherwise geometry
would be smooth while lighting seams remained rigid.

Estimated size: 4–7 engineering days for runtime structures, dual reader/writer,
validation, public builder APIs, and F1 byte-compatibility tests.

## 2. Pose and geometry path

Keep two explicit paths in `geBodyInst_GetGeometry`:

1. **Rigid fast path.** F1 bodies and F2 vertices with one influence retain the
   current bone-sorted loop. The cached object-to-camera/object-to-world matrix
   changes only when `BoneIndex` changes. The arithmetic and output remain
   identical for existing assets.
2. **Weighted path.** At geometry-update time, build one skin matrix per bone
   from the current pose and inverse bind matrix. For each multi-influence
   vertex, transform its bind-space position by each referenced skin matrix,
   multiply by `weight / 65535.0f`, and sum. Use the corresponding rotation
   portions for normals and tangent space, then normalize.

The weighted path should separate single-influence vertices from multi-
influence vertices when the body is finalized. This preserves contiguous
bone-sorted runs for the old path and avoids a four-way branch inside every
legacy vertex. Triangle indices are repaired by the existing vertex-reorder
machinery when either partition is sorted.

The dominant influence (greatest weight, lowest index on a tie) becomes
`ReferenceBoneIndex` only for inherited per-bone lighting and attachment
queries. It does not control the blended position.

Bounds are accumulated from final blended positions. Per-bone authoring bounds
remain useful for coarse queries, but the actor's dynamic extent must come from
the blended output. Invalid matrices or non-finite results fail the geometry
update with a diagnostic; they are not clamped into a plausible pose.

The first implementation remains CPU skinning because the renderer already
consumes CPU-produced actor geometry. A later GPU skinning backend would be a
separate optimization and is not required to make the format correct.

Estimated size: 5–10 engineering days including position, normal, tangent,
bounds, LOD, lighting, shadow, and sanitizer regression coverage.

## 3. Bone budget

There is no measured GPU-uniform bone limit in today's path: posing and
skinning are CPU-side, and pose/XF arrays allocate dynamically. The actual
current limits are:

- `geBody_Index` is signed 16-bit and is used for bone, vertex, normal,
  material, and face-list indices;
- the F1 disk counts and parent/bone indices are signed 16-bit;
- consequently the valid representable positive bone count is at most 32,767;
  and
- `GE_BODY_INDEX_MAX` is currently `0xEFFF` (61,439), which exceeds the signed
  type and is not a valid enforceable limit. The F2 validator must use
  `INT16_MAX` until indices are widened in a separately reviewed format change.

A read-only local compatibility corpus contained 45 actor files. Forty-two
loaded far enough to report bone counts, spanning 1–50 bones; three were
rejected earlier for missing required actor-container sections. This measures
current content, not maximum engine throughput.

For the first supported F2 contract, validate and test 1, 50, 128, and 256
bones. A 256-bone supported budget covers several times the largest measured
asset while keeping corrupt-file allocations bounded. The file can represent
more, but counts above 256 remain unsupported until a generated stress actor is
profiled for pose time, skin time, and memory. Widening all body/face indices is
not part of smooth skinning and would turn this extension into a broader format
rewrite.

Estimated size: 2–3 engineering days for generated limit fixtures and profiling;
index widening, if later required, is a separate 2–4 week migration.

## 4. GLB-to-ACT converter

Add a standalone C++ command-line tool, tentatively `gltf2act`, that consumes
GLB and writes the existing ACT virtual-file container with an F2 BODY Geometry
member. It must not be linked into the runtime.

Pipeline:

1. Parse nodes, meshes, skins, inverse-bind matrices, animation channels,
   materials, images, and accessors with bounds-checked GLB handling.
2. Resolve the selected skin and skeleton root. Reject cycles, duplicate joint
   mappings, missing inverse binds, non-finite transforms, unsupported sparse
   accessor cases, and meshes with no usable skin.
3. Convert coordinates, handedness, units, winding, UV origin, and quaternion
   convention in one documented transform stage. A generated asymmetric basis
   fixture must catch axis swaps and mirrored winding.
4. Read `JOINTS_0`/`WEIGHTS_0`; optionally merge `JOINTS_1`/`WEIGHTS_1`, retain
   the greatest four weights, normalize, and emit F2 influences.
5. Preserve indexed geometry, split vertices only where UV, normal, tangent,
   material, or influence tuples differ, and generate missing normals/tangents
   only under an explicit flag.
6. Convert supported base-color textures to `geBitmap` material members with
   named, reproducible color-space and alpha handling. Unsupported PBR channels
   produce diagnostics rather than being silently discarded.
7. Convert translation/rotation animation channels into named `geMotion`
   objects. Preserve source key times where the Genesis interpolators can
   represent them; otherwise resample at an explicit command-line rate. Scale
   animation is rejected until the motion format has a defined representation.
8. Reopen the produced ACT through the public loader, enumerate bones/motions/
   materials, pose every motion at its endpoints, and reject the output if any
   value is non-finite or any required name is missing.

Use a small, auditable glTF parser rather than writing binary GLB parsing from
scratch. Dependency choice and licence compatibility must be recorded before
implementation, and third-party notices must ship with source.

The converter is generic public tooling. A private manifest can map a converted
actor and its motion names into a game. That same path supplies the replacement
endpoint actor requested under D39; no second bespoke importer is needed.

Estimated size: 18–30 engineering days for geometry/skeleton/weights/materials,
motion conversion, diagnostics, reproducibility, and round-trip tests. This is
a new converter subsystem, not a rewrite of the engine.

## 5. Proof and release gates

The minimum mathematical fixture has two bones in bind pose and one vertex at
`(1, 0, 0)` with equal root/child weights. Rotate the child 90 degrees around
Z while the root remains fixed:

- root result: `(1, 0, 0)`;
- child result: `(0, 1, 0)`;
- expected smooth result: `(0.5, 0.5, 0)`; and
- rigid controls: `(1, 0, 0)` and `(0, 1, 0)`, neither equal to the smooth
  result.

Assert the position within a stated epsilon, the normalized blended normal,
the dynamic bounds, and identical results through write/reload. A visual
fixture renders the same bent two-segment strip twice, rigid and weighted,
from one camera and stores a pixel-difference receipt. Sabotaging the weighted
accumulation to select only the dominant bone must fail both the numeric and
visual fixtures.

Additional gates:

- existing F1 actor corpus produces byte-identical geometry and frame hashes;
- malformed influence counts, indices, sums, floats, and allocation sizes fail
  cleanly under ASan/UBSan;
- 1/50/128/256-bone generated actors load, pose, render, and tear down;
- converter output is deterministic for identical input and options; and
- public Release and matched sanitizer suites remain fully green.

Estimated total: 29–50 engineering days, approximately 6–10 full-time weeks for
one experienced engine developer. The engine portion is a bounded extension.
The converter is substantial new tooling and owns most of the schedule.

## Not covered

No smooth-skinning source, GPU skinning, index widening, GLB converter, or
asset conversion exists yet. The design does not claim visual improvement
until converted weighted content is rendered and compared. It also does not
change the separate Genesis3D startup-logo distribution gate.
