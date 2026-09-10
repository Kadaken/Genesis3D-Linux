# Synthetic actor pose-chain gate

Date: 2026-09-10

## Scope

This asset-free regression separates the legacy ACT/BODY/keyframe stages from
rendering. It creates a three-bone chain, attaches one vertex to each bone,
writes the actor through `geActor_DefWriteToFile`, reloads it, and applies a
known positive and negative quarter-turn to the child chain.

## Exact checks

- BODY/ACT public writer and public reader complete a round trip.
- The three named rotation paths decode to the authored matrices within
  `1e-4`.
- Rest-pose root, child and tip vertices resolve to `(11,0,0)`, `(11,5,0)` and
  `(11,9,0)`.
- Animated root, child and tip vertices resolve to `(11,0,0)`, `(10,6,0)` and
  `(7,5,0)`.
- The checks run through `geActor_GetPoseTransforms` and
  `geBodyInst_GetGeometry`, not a parallel test implementation.

## Finding

The first version of the fixture stopped while reloading its generated body.
The reader had already been converted to the fixed 32-bit BODY 00F1 records,
but the writer still emitted native LP64 structures. The writer now explicitly
encodes every disk record, including the 32-bit bitmap-presence token. With
that symmetry restored, keyframe decoding, two-bone composition, three-bone
accumulation and skinned vertex output all pass.

This gate does not establish that every third-party actor asset is authored
correctly and does not compare rendered pixels.
