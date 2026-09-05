# Project Reports

This folder contains implementation reports, validation results, diff summaries,
and build evidence for the Genesis3D native Linux port. Reports preserve what
was tested at each milestone; later reports may narrow or supersede earlier
descriptions.

## Working rules

- Record substantive results here as ordinary `.md` project reports.
- Engine input capture is disabled by default. Agent-run tests must also set
  `GENESIS3D_NO_INPUT_CAPTURE=1` and must never set
  `GENESIS3D_ENABLE_INPUT_CAPTURE`.
- Never open a test window on DP-1 or over a game. Hardware test windows must
  start on the built-in eDP-1 panel and remain freely movable.
- Never leave the engine running after a test.

## Current scope references

- `VERIFICATION_SCOPE.md` is the controlling summary of verified and incomplete
  engine behavior.
- `COMMUNITY_ENGINE_ROADMAP.md` lists engine-only contribution priorities.
- `PUBLICATION_ACCURACY_LEGAL_AUDIT.md` records the public-site, provenance,
  licensing, and claims review completed before wider promotion.
- `GAME_INTEGRATION_READINESS.md` records the boundary between the working
  native engine sandbox and a complete playable game.
