# Player BSP Collision, Gravity, and Jumping

Date: 2026-09-03

## Result

The Linux player camera now moves through `rfedit1.bsp` with the engine's real
`geWorld_Collision` swept-hull API. Planar input is no longer applied directly
to X/Z. Each 60 Hz simulation tick performs collision-constrained horizontal
motion, gravity and vertical collision, then derives the render view from the
resulting camera position.

Space is tracked from `SDL_SCANCODE_SPACE` through the same explicit held-key
state as WASD. A false-to-true edge applies the jump impulse only while
grounded. Holding Space through landing does not retrigger a jump. Upward hull
contacts zero vertical velocity, so low ceilings reject the remaining ascent.

## Controller geometry and constants

Genesis3D exposes a swept axis-aligned `ExtBox`, not a capsule primitive. The
controller therefore uses a conservative standing capsule approximation with
explicit eye-relative bounds:

- hull minimum: `(-18, -56, -18)` units
- hull maximum: `(18, 8, 18)` units
- maximum stair rise: `18` units
- support snap: `4` units
- collision skin: `0.125` units (in addition to the trace code's native plane
  epsilon)
- walkable floor: normal Y at least `0.707107` (45 degrees)
- gravity: `1200` units/s² downward
- jump impulse: `480` units/s upward; measured unobstructed apex rise is `92`
  units at the canonical fixed step
- maximum slide planes per movement component: `4`

The hull is intentionally axis-aligned, matching the native API and keeping
the player's footprint independent of view yaw and pitch.

## Collision algorithm

Horizontal travel is swept against `GE_CONTENTS_SOLID_CLIP` and world models.
On impact, the controller moves to the reported impact plus skin, removes the
component of remaining travel directed into the collision plane, and repeats
for up to four planes. This provides wall sliding and corner handling without
tunneling at the existing 60 Hz simulation rate.

When grounded horizontal movement encounters a blocking plane, the controller
tries an up/horizontal/down sequence. It accepts that route only when the down
trace finds a walkable surface and the stepped route makes more forward
progress than the direct slide. Sloped BSP planes participate in the same
normal projection and must meet the walkability threshold.

Gravity integrates vertical velocity every fixed tick. Downward contacts on a
walkable plane establish ground, clear downward velocity, and retain the floor
normal. A short downward support trace snaps small descents and maintains a
stable rest height without visible jitter. Unsupported players continue
falling until a swept hull encounters the level bounds. Upward collision with
a ceiling clears positive velocity immediately.

## Deterministic regression evidence

The startup regression runs against the loaded production
`PurificationWorkspace/Assets/Maps/rfedit1.bsp`, not invented test geometry.
It reports:

- floor acquisition: PASS at eye Y `-63.775`
- 120-tick grounded rest with no drift: PASS
- 128-unit fall and landing at the original floor height: PASS
- jump, apex, landing, and held-Space rising-edge suppression: PASS; apex
  `+92.000` units and exactly one jump
- upward ceiling sweep and downward-facing ceiling normal: PASS
- real-map wall collision/slide route: PASS
- real-map step or non-flat walkable slope route: PASS
- two seconds presented at 60 and 120 Hz producing identical fixed-step final
  position: PASS

The SDL queue regression now pushes and consumes both W and Space down/up
events and verifies both held states clear.

## Build and runtime verification

- Production CMake build: PASS.
- Sustained non-capturing 120 Hz hardware run: 1,375 frames / 11.458 seconds,
  exactly 120.00 FPS, zero schedule resynchronizations, 24 render deadline
  misses, 3.347 ms maximum lateness, and one maximum fixed step per frame.
- Clang 22 ASan+UBSan build: PASS. A six-second non-capturing hardware run
  completed at 60.00 FPS with no address or undefined-behavior diagnostic.
- Four actor animation fixtures: PASS.
- Runtime dynamic-light add and byte-exact remove/restore: PASS.
- Stable unattended floor rest: grounded, zero vertical velocity.

Every automated renderer launch used `GENESIS3D_NO_INPUT_CAPTURE=1`,
`GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080`. No automated launch
enabled capture, requested focus, grabbed a pointer, or used DP-1. No engine
process remains.

Implementation commit: `6a6c8b0`.

## Remaining feel tuning

The collision behavior and safety invariants are implemented and regression
tested. Hull radius/eye height, 18-unit step height, 400/800-unit walk/sprint
speeds, gravity, and jump impulse are gameplay-feel values; physical play may
justify tuning them together. The axis-aligned hull is deliberately more
conservative than a true rounded capsule around tight diagonal openings.

## Physical retest

Run exactly:

```bash
cd /home/user/Genesis3D_Project && GENESIS3D_ENABLE_INPUT_CAPTURE=1 ./genesis3d_linux
```

Verify walking into walls and along them, climbing stairs/slopes, walking off
an edge and landing, jumping with Space, holding Space through a landing, and
jumping under a low ceiling. Press Escape to release capture and exit.
