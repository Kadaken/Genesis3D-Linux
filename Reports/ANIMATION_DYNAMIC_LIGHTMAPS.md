# Actor Animation and Runtime Lightmaps

Date: 2026-09-03

## Result

Actor motion playback and runtime lightmap regeneration are integrated into the
native renderer. Animation and light-style phase advance on a canonical 60 Hz
simulation clock independent of the 60/120 Hz presentation cadence. The actor
uses the original `geMotion_GetTimeExtents` and `geActor_SetPose` APIs, loops
within the motion's declared nonzero extent, retains its world root transform,
and safely holds the default pose when no usable motion exists.

World lighting now composites the BSP's real RGB light layers, clamps the
result to RGB8, marks point and spot dynamic-light influence against surface
bounds, and uploads only dirty lightmap textures for currently visible faces.
Previous dynamic influence is retained for invalidation, so moving or removing
a light restores the baked/styled layers. The static face VBO and both UV sets
remain unchanged.

## Completed / pending

| Capability | State | Evidence |
|---|---|---|
| Fixed-clock actor playback | Complete | 60 Hz simulation tick is independent of presentation; 120 Hz run executed 673 animation steps over 11.233 s |
| Motion discovery/extents/looping | Complete | All four fixtures expose usable motion 0 and loop over reported extents |
| Four-fixture posed-vertex validation | Complete | Maximum displacement: 9.1904, 8.2336, 5.4196, and 67.7797 world units |
| Actors without usable motion | Complete | Loader leaves `motion` null and holds the transformed default pose |
| Styled light layer compositing | Complete | Up to four real BSP RGB layers are intensity-composited and RGB8-clamped at a 10 Hz style cadence |
| Dynamic point/spot compositing | Complete | API-driven light on real face 0 brightened its lightmap; remove/restore returned byte-exactly to baked pixels |
| Dirty visible texture updates | Complete | `glTexSubImage2D` updates per-face textures; invisible dirty faces remain pending until visible |
| Immutable VBO preservation | Complete | No geometry regeneration or VBO upload occurs during light updates |
| Active styled-light visual fixture | Pending fixture data | `rfedit1.bsp` reports 0 styled surfaces and 0 configured active styles |
| Persistent dynamic-light visual scene | Pending fixture data | Sterile baseline creates 0 persistent dynamic lights; startup validation adds and removes one through the public API before measurement |
| Shadow-casting dynamic-light occlusion | Pending | Point/spot falloff is ported; BSP trace-based `CastShadow` sampling remains on the legacy renderer path |
| Manual input feel | User validation required | Automated launches deliberately disable focus and input capture |

## Fixture evidence

| Fixture | Motions | Selected | Extent | Maximum posed-vertex delta |
|---|---:|---:|---:|---:|
| `ENTITY01.ACT` | 9 | 0 | 0.042–0.875 s | 9.1904 |
| `ENTITY_0003.ACT` | 5 | 0 | 0.042–0.500 s | 8.2336 |
| `ENTITY_02.ACT` | 7 | 0 | 0.042–0.875 s | 5.4196 |
| `NODE001.ACT` | 8 | 0 | 0.042–0.875 s | 67.7797 |

No fixture lacks animation data. Motion names are intentionally omitted from
diagnostics and this report to preserve the sterile vocabulary boundary.

## Performance and verification

- Production build completed at 100%.
- Clang ASan+UBSan build completed at 100%; historical enum-conversion and
  pragma warnings remain. A safe 60 Hz sanitizer hardware run completed without
  an ASan or UBSan diagnostic.
- Short 120 Hz hardware run: 396 frames / 3.300 s, 120.00 FPS, zero schedule
  resynchronizations, 19 render-deadline misses, 1.537 ms maximum lateness.
- Long 120 Hz hardware run after separating simulation/presentation clocks:
  1,348 frames / 11.233 s, **120.00 FPS**, zero schedule resynchronizations,
  167 render-deadline misses, 6.110 ms maximum lateness.
- 60 Hz hardware run: 257 frames / 4.283 s, 60.00 FPS, zero schedule
  resynchronizations and zero render-deadline misses.
- With no configured runtime styles or persistent lights, measured-frame
  lightmap regeneration is correctly zero. The add/remove startup validation
  exercises real GPU texture updates outside cadence accounting.

Every automated launch used `GENESIS3D_NO_INPUT_CAPTURE=1`,
`GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080` on eDP-1. No automated
launch set `GENESIS3D_ENABLE_INPUT_CAPTURE`; no focus or pointer grab was
requested. No Genesis3D process remained after validation.

Implementation commit: `c6640bb`.
