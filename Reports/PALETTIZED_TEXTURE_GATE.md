# Palettized BSP texture and colour-key gate

Date: 2026-09-10

## Scope

This asset-free fixture exercises both historical legacy-map layouts: raw
8-bit texture indices with a per-texture RGB palette, and raw 24-bit RGB mip
chains. Both travel through bitmap conversion, OpenGL upload, a 50% lightmap,
and transparent keyed-surface handling.

The palette deliberately contains pure red, pure blue, warm brown, neutral
grey and the historical keyed colour. Grey alone cannot hide a red/blue
channel exchange.

## Discriminator

The target game's two map files contain 184 textures. Every texture payload
span is exactly a raw 24-bit, four-mip RGB chain; none carries the later
one-byte format prefix. Before the correction, `World/WBitmap.c` consumed the
first red channel as a format byte and shifted every later channel. One map
then rendered with displaced colours; strict format validation made both maps
fail loudly, proving the format assumption itself was wrong.

After span-based detection, exact lightmapped results are:

| Palette entry | Expected RGB | Read RGB |
|---|---:|---:|
| red | 128/0/0 | 128/0/0 |
| blue | 0/0/128 | 0/0/128 |
| warm brown | 90/45/12 | 90/45/12 |
| neutral grey | 64/64/64 | 64/64/64 |
| keyed colour | 0/0/0 background | 0/0/0 background |
| raw RGB 24/220/48 | 12/110/24 | 12/110/24 |
| raw RGB keyed colour | 0/0/0 background | 0/0/0 background |

The first divergence was BSP texture-payload framing, before either palette
conversion or driver upload. Pixel composers and `GL_RGBA` upload remain
unchanged. The target maps retain palette chunks, but their live texture
payloads are raw RGB; the initial claim that all 97 start-map textures used
palette indices was disproven by the offset/span audit.

## Claim boundary

The fixture proves raw-RGB alignment, indexed palette order, and binary key
transparency through the public legacy frame pipeline. A hidden target-map
stage-4 run now initializes and renders one frame successfully. It does not
claim media decoding, animated video textures, content-authored translucency,
or visual equivalence for every third-party map.
