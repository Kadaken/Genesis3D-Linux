# Sterile Asset Purification

Date: 2026-09-03

## Result

A project-owned asset copy now exists at `PurificationWorkspace/Assets`.
All 47 source links were dereferenced into regular files; the isolated tree has
zero symbolic links. The original `Assets/` link stubs were retained as a
safety measure, and no installed target was modified or deleted. Generated
source-path and hash evidence is kept under
`Reports/evidence/sterile_asset_purification/` and excluded from Git.

The 16 reviewed hash-addressed candidates (C01-C16) were mapped to neutral
ASCII identifiers. Four were actor filenames; their copied `.act` files were
renamed in coordination with every BSP occurrence. The remaining candidates
use generic node, entity, and motion labels. The committed production manifest
contains only candidate IDs, offsets, hashes, lengths, and sterile replacements.

`patch_asset_strings.py --apply` changed 69 fixed-width occurrences in only the
isolated `rfedit1.bsp` and created `rfedit1.bsp.pre-purification.bak` beside it.
Verification established that the BSP remained 15,554,490 bytes, every field is
the exact replacement plus NUL padding, all original candidate byte sequences
are absent, all four renamed references resolve case-sensitively, and every
external source still matches its recorded pre-copy SHA-256.

## Runtime validation

The native engine was run with the isolated project root and the explicit safe
environment settings `GENESIS3D_NO_INPUT_CAPTURE=1`,
`GENESIS3D_WINDOW_X=892`, and `GENESIS3D_WINDOW_Y=1080`. It loaded the renamed
actor and purified BSP, uploaded 6,436 faces, 97 materials, and 7,915 lightmaps,
then sustained 60.00 FPS for the guarded run with zero deadline misses. The
process received TERM through `timeout`, performed normal teardown, and no
engine process remained. No file manager was opened.

Window enumeration did not expose a title matching Genesis3D, so placement was
not independently captured through `wmctrl`; the engine was nevertheless given
the established eDP-1 origin before its first frame and input/focus capture was
hard-disabled. No test was intentionally placed on DP-1.

## Reproduction

Run `tools/asset_purification/prepare_sterile_workspace.py` only when
`PurificationWorkspace` does not exist. It refuses to overwrite an existing
workspace, requires exactly 47 source links, copies with dereferencing, matches
actor names by candidate bytes, and generates `sterile_manifest.json`. Then run
the existing patcher first in dry-run mode and again with `--apply`.

The copied legacy binaries, backups, and generated evidence remain intentionally
untracked and must not be committed.
