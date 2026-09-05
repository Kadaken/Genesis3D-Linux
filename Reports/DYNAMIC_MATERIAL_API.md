# Dynamic world-material API

Date: 2026-09-05

## Purpose

API version 17 adds a narrow renderer boundary for application-owned animated
surfaces. The engine exposes material count, bounded material-name lookup,
dimensions, and replacement of an existing material with RGBA8 pixels. It does
not ingest videos, interpret game entities, or know private asset paths.

## Safety properties

- Material names are bounded and matched case-insensitively against the loaded
  BSP material table.
- Dimensions must exactly match the authored material.
- Width, height, row stride, and byte count are checked for integer overflow and
  insufficient buffers before OpenGL sees the pointer.
- Non-packed source rows are copied into a tightly packed temporary buffer.
- The active texture unit, texture binding, and unpack alignment are restored
  after each update.
- Headless mode validates the same material and memory contract without creating
  an OpenGL context.

## Verification

- Native engine build: PASS (100%).
- Private application headless frame-advance test: PASS.
- Private application Xvfb OpenGL upload test: PASS.
- Clang ASan/UBSan headless and hidden-render lifecycles: PASS with leak
  detection enabled.

The hidden-render test establishes successful OpenGL uploads. It is not a claim
of exact pixel equivalence with a historical Windows renderer.
