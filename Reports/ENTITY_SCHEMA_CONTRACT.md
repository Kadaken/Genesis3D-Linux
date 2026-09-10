# Application-declared entity schema contract

TESTED — generic entity schema and borrowed-name fixtures, including synthetic
world loading. No application-specific compatibility or gameplay claim.

## Ownership and construction

Create an empty `geEntity_EntitySetCreate()` container; call
`geEntity_EntitySetDeclareClass(set, className, fields, count)` for each class;
call `geWorld_CreateWithEntitySchema(file, set)`; destroy the declaration set.
Names, type strings, and present declaration-default strings are copied during declaration. Construction synchronously
builds independent class metadata: the world borrows no declaration memory.
Duplicate class declarations replace the previous declaration atomically.
Duplicate field names, invalid arguments and declaration on populated sets fail.
An unknown field type rejects construction; partial-world cleanup is exercised.

The original `geWorld_Create(file)` delegates with no schema. Registration is
not process-global. This does not make existing world loading reentrant: the
inherited entity parser still has single-threaded construction state.

## Layout and value rules

Application field order determines native offsets/alignment. `FieldSize` ends
at the last field and excludes trailing C-struct padding; use the existing
userdata reservation API for `sizeof(struct)` when needed. Size reservation
alone is not field migration.

- Matching values parse by name into the application type.
- Scalar numeric input into a point becomes `(0, value, 0)`.
- Integer input into a float becomes the numeric floating-point value.
- Value precedence is instance epair, level `%defaultvalue%`, application
  declaration default, then zero. Presence decides precedence, so an authored
  numeric zero wins. All three text sources use the same conversion path;
  declaration-only fields use their declared type as the source type.
- `True` and `False` are accepted case-insensitively for integer-backed Boolean
  declarations in addition to numeric text.
- Added fields with neither a declaration default nor an instance epair remain
  zero. Added fields with instance epairs parse normally even when the old
  level typedef did not declare the field.
- Removed authored fields are ignored and diagnosed once at schema application.
- Unsupported conversions, invalid numeric text, nonfinite/out-of-range values
  remain zero and produce one invalid-value diagnostic per field per class.
- Strings borrow epair values. Typed entity references still resolve names to
  userdata when the application represents them as an opaque pointer. Arbitrary
  runtime pointers remain null. Model references retain named-model resolution.

`geEntity_GetNameString(entity)` borrows the immutable `%Name%` epair string,
valid for entity/world lifetime. Missing entity/name returns null. The existing
copying name API is unchanged.

## Verification

Release and matched Clang ASan/UBSan suites: 14/14. Added tests:
`genesis3d_entity_schema` and `genesis3d_entity_names`.

The generated schema world checks a level-authored 255 default winning over a
declaration default, declaration-only numeric and Boolean defaults, explicit
zero precedence, an invalid-default zero/single diagnostic, an inserted point, scalar-to-point Y value,
integer-to-float value, offsetof agreement, zeroed fields, ignored fields,
invalid-value diagnostic state, live entity links, caller-string mutation after
declaration, schema destruction after construction, failed unknown-type
construction cleanup, and a subsequent unmodified legacy world. The name test
checks persistent pointer identity/string contents after stack-buffer churn.
The existing nine tests, including reference-preserving reservation and the
inherited OpenGL frame fixtures, remain enabled.

Not covered: concurrent world construction, malformed entity binary streams,
application field-rename semantics, behavioral
equivalence of changed content, physical GPU/input testing or production safety.
