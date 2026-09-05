# Game-state boundary API

## Result

The renderer API is version 10. It exposes two input edges and one lifecycle
operation needed by an external game layer while retaining all save data and
serialization outside the public engine.

## Additions

- F9 and F10 become one-poll `QuickSave` and `QuickLoad` edges.
- Key-repeat events cannot create repeated save/load requests.
- `geLinuxRender_ResetPlayerPhysics` clears vertical velocity, contact state,
  held movement, and held fire after a game-owned restore.
- Map replacement also clears pending save/load edges.
- Numeric weapon-slot input is delivered as a one-poll selection edge.
- A renderer-neutral transient beam primitive supports bounded game effects.

The API does not know about save paths, game entities, inventory, or private
assets. Those remain responsibilities of the sibling private game layer.

Beam lifetime advances on the fixed simulation clock. The API rejects non-finite
coordinates/colors, out-of-range colors, invalid width, and invalid lifetime.
The compatibility OpenGL draw restores every render-state category it modifies.

## Safety

The version bump prevents an older caller with a shorter input structure from
being accepted accidentally. Automated verification is performed through the
private runner in headless, no-input mode; no mouse or desktop input is captured.
