# Entity inspection and multi-actor API

Status: **BUILT and tested through the private runner**.

API version 5 introduced bounded, copy-out inspection of BSP entity classes, records,
keys, and values. No engine-owned pointer crosses the shared-library boundary.
The API also supports multiple independently transformed, animated, visible
actor instances and explicit creation, removal, and cleanup.

The engine remains content-neutral: it does not interpret NPCs, health,
weapons, triggers, or level rules. The private application resolves actor files
within its private asset boundary and owns every gameplay decision.

Verification covered 19 authored actor instances on the first private BSP and
34 on the second, success and teardown under Clang ASan/UBSan, and a hidden-Xvfb
OpenGL frame across a map transition. No physical display or input capture was
used.

The current ABI is version 7; subsequent versions add BSP submodel/motion and
per-model render-visibility access without changing these entity/actor
contracts.
