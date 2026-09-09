# Entity userdata reservation — 2026-09-08

**TESTED:** public Release and matched Clang ASan/UBSan suites pass 9/9.
This adds generic storage APIs; it does not migrate schemas or reinterpret
fields whose order or type changed.

## API contract

Declared in Genesis.h, implemented in Entities/Entities.c:

- `geEntity_EntitySetGetClassFieldSize(Set, ClassName, FieldSize)` returns
  the declared class field extent. Reservation never changes this value.
- `geEntity_EntitySetReserveUserData(Set, ClassName, MinBytes)` grows every
  instance of that class to at least MinBytes, preserving current bytes and
  zeroing the new tail. Smaller/repeated requests do not shrink, relocate or
  reset data. Later growth preserves prior tail edits too.

Both require the complete owning entity set, not a class-specific view;
unknown classes or invalid arguments return FALSE. Negative reserve sizes
fail; zero is a no-op for a known class. A failed query leaves output untouched.
Use before handing userdata to application managers, without concurrent access.

Internal geEntity.UserDataCapacity tracks actual allocation extent separately
from Class.FieldSize. All replacement blocks are allocated before publishing
any, so an allocation failure frees only pending replacements. After publishing,
the complete set is scanned to repair declared TYPE_STRUCT and TYPE_PTR fields
that point to replaced userdata, including incoming, mutual and self references.
Old storage is released only after repair. Epairs are not reparsed, preserving
runtime edits and pointer assignments. Model and string pointers are not changed.

External application pointers, interior pointers and undeclared tail fields
are not discoverable and cannot be repaired. Reservations must precede their
use. No application layouts or defaults are introduced into the engine.

## Fixture and evidence

`tests/entity_reserve_test.c` generates a binary entity stream in memory and
loads it through the existing parser. Two instances of a two-field class grow
to a five-field struct; a second class contains incoming typed/raw pointers.
The test checks original values, zero tail, stable declared size, alignment for
engine builtin entity types, self/cross/incoming reference repair, no-op
reservations, and repeated growth preserving edited tail bytes. Unknown
classes, negative size, null arguments and non-owning views are rejected.

Temporarily disabling just the reference-repair assignment makes the sanitized
fixture fail its pointer checks (line 66). Restoring the assignment and rebuilding
both configurations returns the complete public suites to 9/9. No mutation,
sanitizer suppression or expected-failure relabel remains.

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --build build-rf8-sanitize --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-rf8-sanitize --output-on-failure
```

The sanitized directory uses Clang Debug and address/undefined sanitizers.
Build matched engine/consumer configurations because the RAM ABI depends on
NDEBUG. The new fixture needs neither assets, audio nor a window; existing
graphics fixtures remain hidden under Xvfb.

Not covered: allocator-failure injection, malformed schemas, concurrent use,
application-specific migration/defaults, Windows execution or physical GPU/input.
The atomic failure path is source-reviewed, not fault-injected. Reservation
alone is unsuitable when existing fields moved or changed type. No public
push or binary release is made by this pass.
