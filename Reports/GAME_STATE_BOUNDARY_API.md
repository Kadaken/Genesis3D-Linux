# Game-state boundary API

## Result

The renderer API is version 9. It now exposes two input edges and one lifecycle
operation needed by an external game layer while retaining all save data and
serialization outside the public engine.

## Additions

- F9 and F10 become one-poll `QuickSave` and `QuickLoad` edges.
- Key-repeat events cannot create repeated save/load requests.
- `geLinuxRender_ResetPlayerPhysics` clears vertical velocity, contact state,
  held movement, and held fire after a game-owned restore.
- Map replacement also clears pending save/load edges.

The API does not know about save paths, game entities, inventory, or private
assets. Those remain responsibilities of the sibling private game layer.

## Safety

The version bump prevents an older caller with a shorter input structure from
being accepted accidentally. Automated verification is performed through the
private runner in headless, no-input mode; no mouse or desktop input is captured.
