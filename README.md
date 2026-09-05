# Genesis3D Native Linux

A source-available, native 64-bit Linux runtime for the Genesis3D 1.x engine
lineage.

This repository descends from the
[RealityFactory/Genesis3D](https://github.com/RealityFactory/Genesis3D) tree. It
contains Genesis3D 1.1 code released in 1999, subsequent inherited source
snapshots, and Kadaken's 2026 Linux port. The Linux port was developed by Kadaken
with assistance from Gemini and Codex; no outside community contributors to
the port are claimed. Kadaken did not create the original
engine and is not affiliated with or endorsed by Eclipse Entertainment,
WildTangent, Reality Factory, or the owners of games made with Genesis3D.

## Current scope

The native target is an engine runtime and validation sandbox, not a complete
game or a finished replacement for every historical Genesis3D SDK tool.

Verified on one Fedora KDE system with AMD/Mesa:

- native x86-64 C/C++ build;
- SDL2-owned X11/XWayland window and input;
- OpenGL BSP geometry, textures, baked lightmaps, PVS selection, and
  translucent ordering, including transformed door/platform submodels;
- ACT/BODY loading, complete skeletal scale propagation, material alpha,
  fixed-step animation playback, and a generic first-person actor pass;
- runtime lightmap updates;
- swept-hull BSP collision, gravity, jumping, stairs, slopes, wall sliding, and
  ceiling rejection in deterministic real-map fixtures;
- 60 and 120 FPS presentation targets on the tested machine; and
- normal SIGINT/SIGTERM teardown under the documented sanitizer checks; and
- a versioned in-process C renderer API exposed by `libgenesis3d_render.so`.

Known limitations and the exact evidence boundary are documented in
[Reports/VERIFICATION_SCOPE.md](Reports/VERIFICATION_SCOPE.md). In particular,
the project has not established exact pixel equivalence with the original
Windows renderer, broad GPU/distribution support, or completion of every legacy
engine subsystem.

## Licence and release status

The covered source is distributed under the **Genesis3D Public License v1.01**.
Read the complete, controlling text in [g3dlicense.txt](g3dlicense.txt) before
using, modifying, or redistributing this code. This is a custom source-available
licence; the project does not represent it as an OSI-approved licence.

Important obligations include publishing covered engine modifications under the
same terms, documenting modifications, retaining required notices, and using the
original Genesis3D logo as specified by the licence. The native validation
executable does **not yet implement the required original animated startup
logo**. Therefore this repository currently publishes source only and does not
offer a redistributable binary release. Do not redistribute a compiled product,
demo, or application from this branch without independently satisfying every
licence condition.

See [LICENSE](LICENSE), [LEGAL.md](LEGAL.md), [NOTICE.md](NOTICE.md), and
[MODIFICATIONS.md](MODIFICATIONS.md) for discoverability, provenance, and
release safeguards. Those summaries do not replace the controlling licence.

## Assets are not included

No commercial game, map, actor, music, voice, or other legacy game asset is
distributed in this repository. You must provide compatible assets that you are
legally entitled to use.

At runtime, the engine selects the alphabetically first readable compiled level
from `Assets/Maps/` (`.bsp` or `.gbsp`) and requires at least one readable
`.act` file in `Assets/Actors/`. Point `GENESIS3D_PROJECT_ROOT` at a directory
containing that `Assets/` layout. If the variable is unset, the development
harness uses `./Assets` when present and otherwise checks the legacy local
`./PurificationWorkspace/Assets` layout. Neither directory is tracked.

## Build

Fedora dependencies:

```bash
sudo dnf install gcc gcc-c++ cmake make pkgconf-pkg-config \
  mesa-libGL-devel libX11-devel SDL2-devel
```

Ubuntu/Debian dependencies:

```bash
sudo apt install build-essential cmake pkg-config libgl1-mesa-dev \
  libx11-dev libsdl2-dev
```

Configure and compile out of tree:

```bash
cmake -S . -B build
cmake --build build --parallel
```

Compilation alone does not require proprietary game assets. Runtime validation
does.

## Native render library

The CMake build produces both the original `genesis3d_linux` validation
executable and `libgenesis3d_render.so.1`. Applications should include
`Engine/LinuxRender.h` and link the `genesis3d_render` CMake target.

The versioned C ABI provides opaque runtime ownership, exact-map loading,
generic entity queries, multi-actor ownership, full Euler transforms,
motion/scale selection, and application-designated first-person view actors, BSP
submodel lookup and authored
motion sampling, per-model render visibility, SDL input polling, fixed-step BSP
player collision and bounded map-level movement coefficients, bounded
world-material inspection/replacement, explicit
camera submission, simulation advancement, protected world-space and 2D overlay
callbacks, and single-frame rendering. The world callback executes after BSP
geometry with camera matrices active; the renderer restores its OpenGL state and
matrices before continuing. Dynamic
surface decoding and all other game policy remain outside the engine target.

Visible runtimes request a four-sample multisampled framebuffer and enable
OpenGL multisampling when the driver grants it. Window creation automatically
retries without multisampling on systems that cannot provide that format, so
anti-aliasing improves supported hardware without becoming a startup gate.

The intended per-frame order is:

1. `geLinuxRender_PollInput`;
2. application-owned fixed-step state/physics updates, optionally using
   `geLinuxRender_StepPlayer`;
3. `geLinuxRender_SetCamera`;
4. `geLinuxRender_AdvanceSimulation`; and
5. `geLinuxRender_RenderFrame`.

Create the runtime with `Headless = 1` for entity, collision, and lifecycle
tests that must not initialize SDL/OpenGL or capture input.

## Development launch

Headless or agent-run checks must never capture the desktop's keyboard or mouse:

```bash
GENESIS3D_NO_INPUT_CAPTURE=1 \
GENESIS3D_PROJECT_ROOT=/path/to/compatible-assets \
./build/genesis3d_linux
```

For a person intentionally testing input on their own desktop:

```bash
GENESIS3D_ENABLE_INPUT_CAPTURE=1 \
GENESIS3D_PROJECT_ROOT=/path/to/compatible-assets \
./build/genesis3d_linux
```

Controls are W/A/S/D or arrow keys to move, Shift to sprint, Space to jump,
mouse motion to look, and Escape to exit. Interactive mode captures the mouse
only while the engine window has focus and releases it when focus is lost.

## Contributing

Issues and pull requests are welcome. Start with
[CONTRIBUTING.md](CONTRIBUTING.md) and the
[community engine roadmap](Reports/COMMUNITY_ENGINE_ROADMAP.md). Contributions
must be original or properly licensed, must not include commercial game data,
and are subject to the Contributor Grant in the Genesis3D Public License.

## Historical material

Some excluded Windows driver/project files are retained only as historical
reference and are not part of the native build. Files carrying restrictive
third-party SDK/header notices that were unnecessary for the Linux target have
been removed from the current branch. Their former presence in inherited Git
history is disclosed in [LEGAL.md](LEGAL.md).
