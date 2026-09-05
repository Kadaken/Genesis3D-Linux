# BSP submodel rendering and motion API

Date: 2026-09-05

## Result

**BUILT and tested through the private game runner.** Native rendering now
submits the non-root BSP models used for doors and moving platforms instead of
drawing only the static root world.

API version 6 adds bounded world-model count, lookup, bounds, entity-to-model,
motion-extents, motion-sampling, and frame-statistics calls. The engine samples
the original Genesis3D motion path and applies it with
`geWorld_SetModelXForm`, keeping transformed rendering and BSP collision on the
same engine-owned model state.

## Verification

- Standalone engine and shared library clean build: PASS.
- First private map inventory: 19 doors and 7 moving platforms bound.
- Authored motion paths: 26 model bindings sampled and advanced.
- Hidden Xvfb OpenGL run: 12,064 submodel face submissions over eight frames
  (1,508 per frame), with no desktop input capture.
- Clang ASan/UBSan hidden-render and two-map reload runs: PASS with leak
  detection enabled.
- Private asset publication validation: PASS; no game assets are tracked by
  the public engine repository.

## Evidence boundary

This verifies loading, transformed submission, collision-state updates, and
authored path advancement. It does not establish original-renderer pixel
equivalence. Trigger networks, door reversal/closing, and passenger carry on
moving platforms belong to the private game layer and are not claimed by this
engine API milestone.
