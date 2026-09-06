# Portable audio API verification

Date: 2026-09-05

## Decision

OpenAL was selected because it directly expresses dynamic pitch, looping,
per-source gain and spatial pan, plus process-wide listener gain for the legacy
master-volume control.

## Boundary

`SoundOpenAL.c` implements the existing `geSound_*` ABI below `Genesis.h`.
It accepts uncompressed RIFF/WAVE PCM (mono or stereo, 8- or 16-bit) through a
`geVFile`. The historical `Sound.c` remains excluded because it is inseparable
from Win32 DirectSound; portable `Sound3d.c` is now compiled again.

`geSound_GetDSound()` deliberately returns null. This is the only truthful
Linux behavior for a function whose result was a raw DirectSound object.
Downstream code that dereferences the result must be adapted to a portable
streaming API before that path is enabled.

## Verification

- GCC release build: complete, including `libgenesis3d_render.so.1`, the
  standalone validator, and the sound test.
- Null-output test: `genesis3d_sound_null_backend` passed.
- Clang ASan/UBSan build and null-output test: passed with leak detection on.
- No visible window, physical input capture, or physical audio device was used.

The system GCC installation currently points its sanitizer flags at absent
runtime libraries, so the sanitizer pass used installed Clang 22.1.8 instead.
