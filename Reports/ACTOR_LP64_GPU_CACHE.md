# Actor LP64 Geometry and GPU Face Cache

Date: 2026-09-03

## Result

Legacy BODY version `0x00F1` geometry now parses through explicit 32-bit disk
records and converts into native runtime structures. All four purified actor
fixtures parse, create a pose/body instance, expose non-empty geometry and
materials, and submit posed primitives through the native OpenGL actor path.

Immutable BSP face vertices and both UV sets are now uploaded once to a static
OpenGL vertex buffer. Camera PVS selection remains per frame, translucent faces
remain sorted back-to-front, and material/lightmap textures remain selected per
face. Dynamic and animated lighting can therefore invalidate or update the
separate lightmap textures without rebuilding immutable vertex commands.

## Legacy disk layouts

The BODY header type/version and face counts are unsigned 32-bit values;
geometry element counts are signed 16-bit values; LOD flags are signed 32-bit.
The version `0x00F1` element records are:

| Record | Bytes | Fields |
|---|---:|---|
| XSkinVertex | 24 | point `3xf32`, U/V `2xf32`, LOD mask `i8`, one pad byte, bone `i16` |
| Normal | 16 | normal `3xf32`, LOD mask `i8`, one pad byte, bone `i16` |
| Bone | 76 | min/max `6xf32`, transform `12xf32`, parent `i16`, two pad bytes |
| Material | 16 | bitmap-presence token `u32`, RGB `3xf32` |
| Triangle | 14 | vertex `3xi16`, normal `3xi16`, material `i16` |

Compile-time size checks guard these records. The reader performs field-wise
conversion and never overlays a disk record onto a runtime object. This is
essential for Material: its four-byte on-disk bitmap token becomes an eight-byte
runtime pointer on LP64. No packing directive was applied to runtime structs.

The fourth fixture also exposed legacy BMP headers that used host `long` and
the historical `uint32` typedef. Their packed disk fields now use explicit
`int32_t`/`uint32_t`, restoring the required 14-byte file header and 40-byte
info header. Actor face-list bounds now correctly interpret `FaceListSize` as
bytes rather than 16-bit elements; this removed a latent out-of-bounds walk.

## Four-asset fixture results

| Fixture | Vertices | Faces | Normals | Materials | Posed render |
|---|---:|---:|---:|---:|---|
| `ENTITY01.ACT` | 2,900 | 4,746 | 3,835 | 11 | PASS |
| `ENTITY_0003.ACT` | 4,219 | 6,978 | 5,983 | 13 | PASS |
| `ENTITY_02.ACT` | 2,799 | 4,632 | 3,705 | 4 | PASS |
| `NODE001.ACT` | 2,925 | 4,174 | 2,925 | 1 | PASS |

The production renderer continues to draw `ENTITY01.ACT` every frame. A guarded
startup fixture pass loads, poses, validates, and renders all four assets; a
failure prevents the measured loop from starting.

## Performance and validation

- Production CMake build completed at 100%.
- Clang ASan+UBSan build completed at 100%.
- ASan+UBSan hardware run completed without address errors. It found a signed
  overflow in the legacy string checksum; the checksum now uses defined 32-bit
  unsigned wraparound while preserving its legacy bit pattern.
- Final physical-GPU run: 284 frames in 2.367 seconds, **119.99 FPS**, zero
  schedule resynchronizations, 47 render deadline misses, and 6.445 ms maximum
  lateness. It submitted 1,422,840 visible BSP faces, culled 404,984 through
  PVS, and submitted 1,347,864 actor primitives.
- An earlier 10.220-second VBO run reached 118.69 FPS; retaining frame scratch
  vector capacity removed recurring allocations and recovered the stable target
  cadence in the final run.
- Every launch set `GENESIS3D_NO_INPUT_CAPTURE=1`, `GENESIS3D_WINDOW_X=892`,
  and `GENESIS3D_WINDOW_Y=1080`. No launch enabled interactive input capture.
  No Genesis3D process remained after validation.

## Remaining limitations

The 120 FPS cadence target passes at 119.99 FPS within measurement precision,
but the compositor/scheduler zero-jitter criterion still does not pass. Actor
motion time advancement and runtime dynamic/animated lightmap regeneration are
separate pipeline milestones. The BODY writer still predates this Linux bridge;
this milestone makes the asset-loading/render path explicit and byte-correct.

Implementation commit: `6c444fe`.
