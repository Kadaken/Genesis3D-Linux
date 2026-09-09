# Findings outside the current gate

- 2026-09-09: Support/ERRORLOG.C geErrorLog_AddExplicit never assigns ErrorList[].ErrorID, although geErrorLog_Report returns that field; Report indexes oldest-first despite the header promising newest-first. The actor rejection fixture observes and forwards the numeric AddExplicit boundary instead. Logging implementation unchanged.
- 2026-09-09: Actor/body.c geBody_WriteGeometry still writes native struct sizes while its reader uses fixed disk records; nonempty-body round trips need a separate portability audit. The rejection fixture uses an empty generated body and a successful read control, not a general actor writer validation.
