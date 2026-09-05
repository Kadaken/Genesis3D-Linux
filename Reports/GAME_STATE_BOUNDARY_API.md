# Game-state boundary API

## Result

The renderer API is version 12. It exposes bounded input edges, one lifecycle
operation needed by an external game layer while retaining all save data and
serialization outside the public engine.

## Additions

- F9 and F10 become one-poll `QuickSave` and `QuickLoad` edges.
- Key-repeat events cannot create repeated save/load requests.
- `geLinuxRender_ResetPlayerPhysics` clears vertical velocity, contact state,
  held movement, and held fire after a game-owned restore.
- Map replacement also clears pending save/load edges.
- Numeric weapon-slot input is delivered as a one-poll selection edge.
- E is delivered as a one-poll, repeat-filtered `Use` interaction edge. The
  public renderer does not assign gameplay meaning to that edge.
- F12 is delivered as a one-poll `Screenshot` edge. The caller can use
  `geLinuxRender_SaveScreenshot` to save the displayed framebuffer as a 24-bit
  BMP at a caller-owned path.
- A renderer-neutral transient beam primitive supports bounded game effects.

The API does not know about save or screenshot directories, game entities,
inventory, or private assets. Those remain responsibilities of the sibling
private game layer.

Beam lifetime advances on the fixed simulation clock. The API rejects non-finite
coordinates/colors, out-of-range colors, invalid width, and invalid lifetime.
The compatibility OpenGL draw restores every render-state category it modifies.

## Safety

The version bump prevents an older caller with a shorter input structure from
being accepted accidentally. Automated verification is performed through the
private runner in headless, no-input mode; no mouse or desktop input is captured.
