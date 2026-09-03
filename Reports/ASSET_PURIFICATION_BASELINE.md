# Genesis3D Asset Purification Baseline

Date: 2026-09-03

## Result

The native Linux port is now anchored by commit
`780f84db569c6886dcc212616a1f622930d0e988` and annotated tag
`pqpw-release-baseline`. The commit contains the audited source/header work,
portable VFile backend, repository-wide case-sensitive include corrections,
`linux_main.cpp` render/cadence/teardown optimizations, sanitizer policy, and
validation documentation. It contains 183 changed paths with 2,392 insertions
and 967 deletions. Runtime asset links, game binaries, in-source CMake output,
caches, screenshots, logs, and generated string evidence were excluded.

The pre-commit checks passed: `git diff --cached --check` was clean and the
native CMake target built to 100%. No engine or graphical application was
launched.

## Asset inventory

`Assets/` contains 47 symbolic links: 45 `.act` actor files and two compiled
`.bsp` maps. The links resolve into the separately installed legacy game data;
the files are not repository content. A recursive extension scan found no
`.ini`, `.pawn`, `.pwn`, `.inc`, `.amx`, or `.sma` files, so there are no plain
text INI or PAWN scripts in the project asset layout to purify at this stage.

The reviewed map is `Assets/Maps/rfedit1.bsp`, a 15,554,490-byte binary whose
SHA-256 is
`e20d825bffc7dda8db9ac89822a01ae646b22444da99dcc666b45c5de751ea32`.
The link target was not modified.

## BSP hexadecimal string evidence

The command below generated a complete 155,497-line, 5,275,728-byte offset
dump:

```sh
strings -a -t x Assets/Maps/rfedit1.bsp \
  > Reports/evidence/asset_purification/rfedit1.strings.hex.txt
```

The dump SHA-256 is
`fe0b0a49500ba3aefbf04caf9308d0664fb708266a47717c05bb07391b299223`.
It is retained locally for review but ignored by Git as generated evidence.

A conservative scan identified 69 actionable occurrences in 16 unique ASCII
strings. To avoid needlessly reproducing hostile legacy labels, the table uses
stable candidate IDs and exact SHA-256 identifiers. Byte lengths exclude the
terminating NUL; offsets are the first string byte.

| ID | Bytes | SHA-256 | Hex offsets |
|---|---:|---|---|
| C01 | 7 | `776c2406629afdcbeb1a04b098756b81dc320e6b3bd85b56c41d459bf21825df` | `0xde8de`, `0xeed92` |
| C02 | 14 | `f9cf4eeccbce15075edc3c2112b52b9c5a780e641e1d32c7c949b8e5e54a9fae` | `0xe3998`, `0xe3def`, `0xe4246`, `0xe46a3`, `0xe4afc`, `0xeb75a`, `0xebb50`, `0xebf5b`, `0xec36a`, `0xec777`, `0xecb84`, `0xecf92`, `0xed42c`, `0xed823`, `0xedc19` |
| C03 | 14 | `49143c160fd67a0e080c0a27bcba4f786eb424c30c24a8df9754193455ff9fea` | `0xe39b7`, `0xe3e0e`, `0xe4265`, `0xe46c2`, `0xe4b1b`, `0xeaad1`, `0xeaefd`, `0xeb779`, `0xed44b`, `0xed842`, `0xedc38` |
| C04 | 15 | `1c41b6fb9345da6082bb811e7ef8beec51a707b68361d1ca9627c63bdc8d78de` | `0xe39e9`, `0xe3e40`, `0xe4297`, `0xe46f4`, `0xe4b57`, `0xebba6`, `0xebfb1`, `0xec3c0`, `0xec7cd`, `0xecbda`, `0xecfe8` |
| C05 | 17 | `d579792749a6065ad194dad6d13591bbf41054e6107f48565d029322ef089004` | `0xeaaaf`, `0xeaedb` |
| C06 | 15 | `65bacb528e3faff9a193e939f4439d6bba71e35d6838d32625322fa4deed936f` | `0xeaaf1`, `0xeaf1d` |
| C07 | 15 | `1c48b7dfc69c337c43bebf31e0587f9bae58c6ece0c7530113c00d4c8039df2d` | `0xeab28`, `0xeaf54` |
| C08 | 22 | `dee7a0c0d8cbac5babc591b3817ed15d8ca3ad68f523822f281a9bae9243c522` | `0xeab4f`, `0xeaf7b` |
| C09 | 12 | `116f5b1577819ff42862604f8c1d8ae8b7cb18aeed2ade45d518a3b54b86fd09` | `0xeadb3`, `0xeb1df` |
| C10 | 13 | `ce9084377b1bd7e30a768f68c2c1eb69a1649adf52bafdd89866a033ff38d7a3` | `0xeba2b`, `0xed6fd`, `0xedaf3`, `0xedeea` |
| C11 | 19 | `0be6f5eff8c8d4ac179acebb11540300949dc645c947a1f460c0b6ccbbb6bd34` | `0xebb6f`, `0xebf7a`, `0xec389`, `0xec796`, `0xecba3`, `0xecfb1` |
| C12 | 15 | `78b5c6d8073dbe71035f5c609290d9a363d3ef591e5348a31b1c75f45930ca49` | `0xebe34`, `0xec242`, `0xec64f`, `0xeca5c`, `0xece6a`, `0xed278` |
| C13 | 15 | `735fd7347d0b1b118ff361b941b9630eee7a8b193e301692f9c1447255dbdc32` | `0xee5aa` |
| C14 | 12 | `9121687e949c98df70989d1f6752f7fe8ed4417b48994d246ba83b6582004e16` | `0xee5ca` |
| C15 | 13 | `337e563eff3c19ad00cc8da839f3124609c566db6b44f7a0c7438acf9487bf71` | `0xee614` |
| C16 | 11 | `49fc63a3e0ed191eddba7fd356a5e2c2e4a1e1cec4d929240541c33825171013` | `0xee899` |

Three other lexical hits at `0x28f1`, `0x1b94dc`, and `0xd09783` were rejected
as incidental printable sequences inside binary data, not asset identifiers.
This distinction is important: blind search-and-replace would corrupt the map.

## Safe patch draft

`tools/asset_purification/patch_asset_strings.py` consumes a JSON manifest and:

- resolves and reports both logical and real target paths;
- refuses external symlink targets unless explicitly approved;
- opens the validated target in `rb+` mode;
- verifies an optional whole-file SHA-256 and a mandatory SHA-256 for every
  original byte range before any write;
- rejects negative, out-of-bounds, overlapping, non-ASCII, NUL-containing, or
  overlength replacements;
- pads every shorter clean space-shooter replacement with `\x00` to the exact
  original length;
- defaults to a no-write dry run;
- requires `--apply` to write and creates an exclusive, byte-for-byte backup
  first; and
- flushes with `fsync` and verifies that file size did not change.

`tools/asset_purification/manifest.example.json` documents the manifest shape.
The reviewed production manifest should use the candidate hashes above and
clean labels such as `NAVNODE`, `DRONE_ATTACK`, `RAIDER_DOWN`,
`SENTRY_UNIT.ACT`, and `ACE_BOSS_WALK`, provided each replacement fits its
record length. A production manifest was deliberately not activated because
actor filenames and animation references must be renamed as one coordinated
set; patching the BSP alone would leave broken references.

The script passed a temporary-fixture test in both default dry-run and explicit
apply modes. The test confirmed exact backup content, correct NUL padding, and
an unchanged 30-byte file length. No real asset was opened for writing by the
test.

## Next review gate

Before applying a production manifest, approve the complete replacement
vocabulary and coordinated actor-file rename map. Then copy the external game
assets into a project-owned purification workspace, update every duplicate BSP
reference, run a second `strings` scan, and validate the purified copy in the
engine. The installed originals should remain immutable.
