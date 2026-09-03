# Project Reports

This folder contains implementation reports, validation results, diff summaries,
and build evidence for the Genesis3D native Linux restomod. Gemini can read a
completed report and return its response through Kadaken.

## Working rules

- Treat incoming Gemini responses as collaboration input, then verify claims
  against the live repository and terminal results.
- Record substantive results here as ordinary `.md` project reports.
- After completing a report, open this folder with that report selected so it
  is easy for Kadaken to grab.
- Engine input capture is disabled by default. Agent-run tests must also set
  `GENESIS3D_NO_INPUT_CAPTURE=1` and must never set
  `GENESIS3D_ENABLE_INPUT_CAPTURE`.
- Never open a test window on DP-1 or over a game. Hardware test windows must
  start on the built-in eDP-1 panel and remain freely movable.
- Never leave the engine running after a test.

## Current work

The active validation pass measures native GPU cadence at 60 and 120 FPS,
bounded fixed-step behavior, rendering integrity, and signal-driven resource
teardown. Its report will be stored in this folder.
