# Defined 32-bit pixel composition — 2026-09-09

TESTED: public Release 13/13 and matched Clang ASan/UBSan 13/13, serial.
Six composers (RGBX, BGRX, RGBA, ARGB, BGRA, ABGR) now cast each used channel
to uint32 before shifting and combine nonoverlapping fields with bitwise OR.
Signatures, assertions, byte order and in-range channel behavior are unchanged.

The public-API test composes/decomposes every combination of channel values
{0, 127, 128, 182, 255}: 625 per format, 3,750 total. An independent base-256
word construction also checks packed order, so paired composer/decomposer
swaps cannot pass together. RGBX/BGRX intentionally ignore input alpha, leave
the X byte zero and decode alpha as 255; this is checked for every input alpha.

Before the fix, running the fixture independently for each format triggers
UBSan at top-byte value 128 in all six original expressions, not just one.
After the fix the combined fixture and all existing public tests pass, with
no sanitizer suppression. No other pixel formats or renderer logic changed.

```sh
cmake --build build -j4
ctest --test-dir build -j1 --output-on-failure
cmake --build build-rf8-sanitize -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-rf8-sanitize -j1 --output-on-failure
```

The fixture executable accepts an optional format label (e.g. ABGR) to isolate
one composer. Local receipts are under ignored Reports/evidence/d19/.

Not covered: out-of-range channels, malformed buffers, other pixel formats,
physical rendering, driver/platform diversity or Windows execution. This is
defined arithmetic for valid 8-bit channels, not a new input-validation policy.
