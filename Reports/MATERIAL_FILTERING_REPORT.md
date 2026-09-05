# World-Material Filtering Report

Date: 2026-09-05

## Outcome

World material textures now generate mipmaps after upload and use trilinear
minification. When `GL_EXT_texture_filter_anisotropic` is available, filtering
uses the lesser of the device limit and 8x. This reduces distant texture crawl,
shimmer, and oblique-surface blur without changing source artwork.

The code resolves the core or extension mipmap entry point at runtime. Drivers
without either entry point retain the prior bilinear path. Animated material
replacement regenerates the bound texture's mip chain immediately after its
base level changes, preventing stale imagery at a distance. Lightmaps remain on
their established linear path because their small atlas-like surface regions
must not bleed across generated levels.

## Verification

- Clean standalone and shared-library GCC build: PASS.
- Isolated Xvfb/Mesa context reported trilinear mipmaps and 8x anisotropy: PASS.
- Authored 725-frame dynamic texture binding continued updating: PASS.
- Complete private consumer validation matrix: PASS (59/59 targets).
- Clang ASan/UBSan with leak detection across animated texture, weapon impact,
  world callback, and teardown paths: PASS.
- No physical display or input capture was used.
