# Actor rejection cleanup — 2026-09-09

TESTED: Release 12/12 and matched Clang ASan/UBSan 12/12, serial CTest.
Runtime change is three lines in geActor_DefCreateFromFile: clear the closed
Header and Body handles; route missing Motions through existing cleanup.
The same ERR_ACTOR_FILE_READ and NULL result are preserved.

The synthetic public-VFS fixture generates a valid empty-body control and three
rejected archives: missing Motions, missing required Body, and no declared Body
with missing Motions. It never loads external assets or opens a window.

Before the fix, all acceptance/error checks pass but LSan finds 1,845 bytes in
21 allocations. With cleanup enabled but the closed Body handle deliberately
left non-NULL, ASan reports heap-use-after-free. Both corrected handles and
cleanup routing restored: fixture passes without sanitizer/leak findings.

The test observes the numeric geErrorLog_AddExplicit boundary and forwards to
the real logger: geErrorLog_Report currently reads an ErrorID field that the
logger never assigns. That separate defect is recorded in
FINDINGS_OUTSIDE_SCOPE.md, not patched. Its error-history ordering also differs
from the header description. No logging errors are suppressed.

Not covered: malformed actor fuzzing, nonempty-body writer/reader round trips,
motion-allocation failure cleanup, other Body-reader error paths, Windows
execution or physical rendering. The empty-body valid control establishes only
the generated fixture's parseability. No broader actor-loader completion claim.

Reproduce using matched configured build trees:

```sh
cmake --build build -j4
ctest --test-dir build -j1 --output-on-failure
cmake --build build-rf8-sanitize -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-rf8-sanitize -j1 --output-on-failure
```

Local ignored receipts: Reports/evidence/d18/. The process changes to its own
temporary directory for archive generation; old negative logs may contain an
LLVM-symbolizer relative-library-path warning, independent of the sanitizer
finding. Successful tests return 0 and remove their generated archive/directory.
