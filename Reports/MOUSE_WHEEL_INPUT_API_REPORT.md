# Mouse-Wheel Input API Report

Date: 2026-09-05

## Outcome

The version-21 Linux render ABI exposes bounded, edge-counted `WeaponNext` and
`WeaponPrevious` input values. The renderer translates SDL mouse-wheel events
without embedding inventory or game policy in the public engine.

SDL 2.32 preserves synthetic high-resolution wheel motion in `preciseY` while
the legacy integer `y` field may be zero. The implementation therefore uses
the precise value when available and retains the integer field as a compatible
fallback. It also honors SDL's flipped-wheel direction flag and caps queued
edges at eight per poll to prevent unbounded input accumulation.

## Verification

- Shared-library and standalone GCC build: PASS.
- Isolated SDL event-queue delivery under Xvfb: PASS.
- Both normal directions reached the private consumer through the public ABI:
  PASS.
- Complete private consumer validation matrix: PASS (63/63 targets).
- Clang ASan/UBSan with leak detection for the headless policy and isolated SDL
  delivery paths: PASS.
- No physical mouse, display, audio output, or input capture was used.
