# VFile line and write contracts — 2026-09-08

**TESTED:** public Release and matched Clang ASan/UBSan suites pass **7/7**.
No external assets, physical window, or input capture are needed by the two
new VFile fixtures. Existing graphics tests remain hidden under Xvfb.

## Implementation

`VFile/fsposix.c::Posix_GetS` reads at most caller capacity minus one raw bytes,
returns FALSE on a zero-byte read, scans for CRLF/CR/LF within that read,
folds the terminator to LF/NUL, and seeks back any unused bytes. Without a
terminator it returns the consumed, NUL-terminated fragment as TRUE. Final
unterminated and buffer-filling fragments are deliberate extensions beyond
the old Windows reader's FALSE-with-consumed-bytes behavior.

`VFile/fsvfs.c::FSVFS_GetS` distinguishes remaining member bytes from output
capacity. It limits backend capacity to the smaller of caller capacity and
remaining bytes plus one, computing the addition only when bounded below the
caller capacity. Caller capacity <= 1 and actual member EOF return FALSE.
One remaining byte with capacity >= 2 is returned successfully; the next call
returns FALSE at true EOF. A saved pre-read member length guards the position
assertion, because UpdateFilePos can otherwise enlarge the reported length.

`FSVFS_Write` no longer modifies const input. Mask zero uses the direct write
path; nonzero masks use 4,096-byte stack scratch chunks. The input remains
untouched, and the success-position assertion is conditional on write success.

## Public fixtures

`tests/vfile_contract_test.c` is run as two CTests:

- `genesis3d_vfile_lines`: 12 data/capacity cases through both DOS and VIRTUAL
  backends. Includes CRLF, LF, bare CR, unterminated final text, capacity 2,
  one-byte members, long-line continuation, empty members and final newline.
  Capacities -1, 0 and 1 must neither read nor alter the buffer. Every TRUE
  must advance Tell, remain within Size and return bounded NUL-terminated
  text. A following archive member detects reading beyond the current member.
- `genesis3d_vfile_const_write`: string-literal writes and a 9,001-byte
  multi-chunk payload, each with zero and nonzero masks. Verifies unchanged
  writable input and exact readback after closing/reopening the archive.
  Literal writes run under ASan as well as Release.

Both suites also run the existing sound, texture-query, SDL-window and two
legacy-frame tests. All seven pass in each matched configuration. After adding
the invalid-capacity checks, both focused VFile suites were rerun and pass 2/2.

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-rf8-sanitize --output-on-failure
```

The sanitized build uses Clang, Debug, and
`-fsanitize=address,undefined -fno-omit-frame-pointer` for C and C++.

## Limits

No physical GPU/input, Windows execution, disk-error injection, encrypted
GetS decoding, or application-level completeness is claimed. The bounded
scanner coalesces CRLF only when both bytes are in the same raw read: a CRLF
pair split exactly between calls is treated as separate terminators. There is
no cross-call pending-CR state or lookahead beyond the read budget. This
limitation is distinct from preserving bytes and guaranteeing forward progress.

No public push or binary release is made by this validation pass.
