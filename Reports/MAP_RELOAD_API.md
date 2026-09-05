# Native map reload API

Status: **BUILT and tested**.

The versioned render boundary exposes `geLinuxRender_LoadMap`. The call parses
and prepares a replacement BSP before releasing the active world. A failed
parse or GPU-cache build leaves the prior world active. A successful reload
resets world-dependent lighting, collision, input, diagnostics, and
initial-camera state while preserving the SDL window, OpenGL context, overlay
callback, and loaded actor fixture.

This is deliberately a generic engine operation. Level ordering and game-state
transitions remain owned by the private game layer.

Verification completed against both private BSP aliases through the private
runner: headless success-path transition, failed-load rollback, hidden-Xvfb
OpenGL texture/geometry cache replacement, and Clang ASan/UBSan lifecycle all
pass. No physical display, mouse capture, or physical audio was used.
