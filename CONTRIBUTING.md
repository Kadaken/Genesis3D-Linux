# Contributing to Genesis3D Native Linux

Thank you for helping maintain the engine. Issues, hardware test reports, and
focused pull requests are welcome.

## Read the licence first

All covered contributions are subject to the Contributor Grant and other terms
in the Genesis3D Public License v1.01 (`g3dlicense.txt`). By submitting a
contribution, you certify that:

- you created it or otherwise have the right to submit it under those terms;
- you understand that it will be publicly distributed under the Genesis3D
  Public License v1.01;
- it contains no commercial game data, leaked source, proprietary SDK code, or
  other material you lack permission to distribute; and
- you have described any relevant third-party licence or intellectual-property
  claim.

Do not submit maps, actors, textures, audio, executables, or extracted content
from commercial games. Neutral fixtures must be original and explicitly
licensed for redistribution.

## Before opening a pull request

1. Keep changes focused on the engine or its development tools.
2. Preserve all existing copyright and licence notices.
3. Add the Genesis3D licence notice and your contributor identification to new
   Covered Code where the file format permits it.
4. Add a dated entry to `MODIFICATIONS.md` for material engine changes.
5. Build out of tree with `cmake -S . -B build` and
   `cmake --build build --parallel`.
6. State what hardware, distribution, display server, and GPU driver you tested.
7. Do not claim support or equivalence that your evidence does not establish.

## Testing safety

Automated and agent-run tests must set `GENESIS3D_NO_INPUT_CAPTURE=1`. They must
not force focus, confine a user's pointer, or open over an occupied display.
Only a person intentionally performing interactive testing should set
`GENESIS3D_ENABLE_INPUT_CAPTURE=1`.

## Review

Submission does not guarantee acceptance. Maintainers may request evidence,
licence clarification, narrower scope, or changes needed to preserve legacy
asset compatibility and the documented safety constraints.
