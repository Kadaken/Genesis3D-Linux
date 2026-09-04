# Prior Genesis3D Linux Port Research

Date: 2026-09-03

## Conclusion

I found multiple preservation and modernization attempts, but no publicly
documented earlier project that demonstrates a completed native Linux runtime
for the original 1999 Eclipse/WildTangent Genesis3D 1.1 engine with the same
scope as this repository: native 64-bit build, Linux window/input ownership,
OpenGL BSP and lightmap rendering, ACT parsing and skeletal animation, BSP
collision, and a working interactive camera.

That is a qualified research conclusion, not proof that no private, lost, or
poorly indexed port ever existed. Based on the public repositories and project
records currently discoverable, Kadaken's project appears to be the first
publicly documented working port at this level of completeness.

## Closest direct attempts

### Aeon3D

[Aeon3D](https://codeberg.org/hogsy/Aeon3D) is the closest direct modernization
of Genesis3D 1.1. Its current status says its demonstrated runtime is Windows
11 x86 with Direct3D/Glide. Linux/Unix compilation and the replacement OpenGL
driver are still marked in progress in its
[README](https://codeberg.org/hogsy/Aeon3D/src/branch/master/readme.md).
Its OpenGL driver build also still links `OpenGL32`, confirming that the public
tree does not provide a finished Linux display/render path.

Verdict: directly relevant and valuable prior work, but not a completed native
Linux runtime.

### Styx3D

[Styx3D](https://github.com/8bitprodigy/Styx3D) is a modernization effort based
on Jet3D, a related descendant rather than the same Genesis3D 1.1 engine tree.
Its [README](https://github.com/8bitprodigy/Styx3D/blob/main/README.md) reports
Linux compilation and an SDL3 port in progress, while window creation, drawing,
and an OpenGL backend remain unchecked tasks. Its Makefile currently targets a
shared library rather than a demonstrated game runtime.

Verdict: related cross-platform work, but a different engine branch and not a
rendering Linux runtime yet.

### Paradox Genesis3D modernization

[paradoxnj/Genesis3D](https://github.com/paradoxnj/Genesis3D) contains a modern
CMake layout and converts much of the source to C++, but its public project
still carries DirectX dependencies and does not document a native Linux window,
renderer, or runnable Linux demonstration. The
[engine CMake target](https://github.com/paradoxnj/Genesis3D/blob/master/source/Genesis/CMakeLists.txt)
builds a static core library without a Linux platform/render integration.

Verdict: substantial compiler/build modernization, but no verified complete
Linux runtime.

### RealityFactory repositories and OpenGL driver

The maintained [RealityFactory/Genesis3D](https://github.com/RealityFactory/Genesis3D)
repository preserves and updates the original engine source, but does not
present a native Linux runtime. The separate
[Genesis3D OpenGL driver](https://github.com/ParadoxSoftware/genesis3d-opengl-driver)
is an OpenGL renderer for RealityFactory's Windows engine environment; an
OpenGL backend by itself is not a Linux platform port.

Verdict: these are the primary upstream and an important renderer precedent,
but neither establishes the end-to-end Linux platform layer completed here.

## Important same-name project

[matiaslavik/Genesis-3D](https://codeberg.org/matiaslavik/Genesis-3D) explicitly
advertises desktop Linux support. It is not a port of the 1999 Genesis3D 1.1
engine used by this project. Its
[README](https://codeberg.org/matiaslavik/Genesis-3D/src/branch/main/README.md)
identifies it as a fork of Changyou's separate 2013 “Genesis-3D” engine. That
project also remains WIP and says Linux cannot run built projects, only editor
projects.

Verdict: a real Linux port, but of a different engine with the same name; it is
not prior art for porting the Eclipse/WildTangent Genesis3D 1.1 runtime.

## Other public copies

GitHub contains archival SDK copies and lightly described forks such as
`rainsome-org1/genesis3d`, `pixelmagicgames/genesis3d`, and
`pixelmagicgames/genesis3d_1`. Their public trees retain Visual Studio/DSP/DSW
or Windows make-project layouts and provide no documented native Linux runtime.
The `madeso/genesis3d` project explicitly describes itself as a header/API
exploration that is not intended to compile or run.

## What is novel in the Kadaken port

The public [Kadaken Genesis3D Linux repository](https://github.com/Kadaken/Genesis3D-Linux)
currently documents and implements the combination not found in the projects
above:

- native x86-64 Linux C/C++ build;
- SDL-owned XWayland/OpenGL window, context, and physical input path;
- BSP/PVS rendering with materials, lightmaps, translucency, and dynamic
  lightmap updates;
- explicit 32-bit ACT/BODY disk records converted safely to LP64 runtime data;
- skeletal actor animation;
- swept-hull BSP movement, stairs, gravity, and jumping; and
- fixed-step simulation with measured 60/120 FPS hardware validation.

## Search limits

The audit covered GitHub repository/code search, GitLab project search,
Codeberg project search and current source trees, relevant RealityFactory
repositories, and the active Codeberg successors of archived GitHub projects.
Historical forum posts, private repositories, vanished SourceForge files, or
unindexed personal archives may contain additional experiments. Therefore the
responsible public claim is “first publicly documented complete port found,”
not an absolute “first ever.”
