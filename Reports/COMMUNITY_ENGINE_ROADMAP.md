# Genesis3D Native Linux — Community Engine Roadmap

Date: 2026-09-03

## Status

The project is a working native Linux port and modernization of the Genesis3D
engine. It builds as a 64-bit ELF executable and its implemented rendering,
asset loading, skeletal animation, input, collision, movement physics, dynamic
lightmaps, timing, and teardown paths have been tested.

Genesis3D is an engine, not a game. Game-specific systems and content—including
weapons, enemies, objectives, story, levels, HUD design, and game balance—belong
in a separate game project and are not engine completion requirements.

## Highest-value community work

### 1. Reproducible builds and releases

- Add CI for GCC and Clang on current Fedora and Ubuntu releases.
- Run automated ASan and UBSan jobs on every proposed change.
- Produce versioned Linux release archives or packages.
- Provide redistributable neutral BSP and ACT fixtures so contributors can run
  tests without proprietary game assets.
- Document dependency versions and a clean-machine build procedure.

### 2. Renderer completeness

- Complete area-portal traversal and full polygon frustum clipping.
- Add BSP trace-based shadow occlusion for runtime dynamic lights.
- Build reference scenes for animated light styles, persistent dynamic lights,
  translucent surfaces, fog, and actor materials.
- Create image-comparison regression tests for those scenes.
- Gradually replace compatibility-profile immediate-mode paths with modern
  vertex/index buffers and shaders while preserving asset compatibility.

### 3. Platform coverage

- Test native Wayland operation in addition to the current SDL/XWayland path.
- Test Intel and NVIDIA drivers alongside the verified AMD/Mesa configuration.
- Audit endianness and alignment assumptions before attempting ARM64 support.
- Add robust fullscreen, multi-monitor, DPI, focus, and display-mode tests.

### 4. Engine services

- Implement and test a maintained Linux audio backend.
- Add gamepad input, configurable bindings, and accessible input settings.
- Formalize resource streaming, cache limits, and diagnostic telemetry.
- Add structured logging and actionable loader errors for damaged assets.

### 5. Asset and authoring pipeline

- Finish and validate the legacy BODY writer/export path.
- Create maintained Blender import/export tooling for ACT/BODY/MOTION data.
- Port or replace map compilation and inspection tools needed by creators.
- Document the supported BSP, ACT, texture, lightmap, and VFS formats.

### 6. Security and maintainability

- Fuzz BSP, ACT/BODY, bitmap, and VFS parsers using malformed inputs.
- Continue replacing implicit-width serialization structures with explicit
  on-disk record types.
- Add unit tests around binary bounds checking and allocation failure paths.
- Document source provenance and confirm the licensing status of every bundled
  component before distributing release packages.
- Add contribution, security-reporting, and coding-style documentation.

## Useful contribution tiers

| Priority | Outcome |
|---|---|
| Release gate | CI, neutral test assets, clean-machine build, licensing/provenance documentation |
| Engine completeness | Remaining visibility, lighting, audio, input, and authoring paths |
| Broader support | Wayland, more GPU vendors, more distributions, eventual ARM64 |
| Modernization | Buffered rendering, shaders, improved tooling, profiling and performance work |

## Not part of the engine roadmap

These may be built with Genesis3D, but they belong to a game or framework layer:

- Game rules, missions, characters, enemies, weapons, scoring, and balance.
- A particular game's menus, HUD, save format, dialogue, music, or story.
- Replacement art, maps, models, animations, and other game content.

## Bottom line

The engine works as a tested native Linux runtime sandbox, but it is not a
finished, broadly supported SDK. Community help is most valuable in making the
build reproducible, completing the remaining engine subsystems, strengthening
asset tools, expanding hardware/platform coverage, and maintaining automated
regression tests.
