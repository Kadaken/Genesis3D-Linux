# Genesis3D Native Linux Git Main Baseline

Date: 2026-09-03

## Result

The existing repository history was preserved and audited rather than replaced
with a new `.git` directory. The primary branch is now named `main` and is
published as a public GitHub repository:

```text
https://github.com/Kadaken/Genesis3D-Linux
```

Baseline commit (its original subject used "feature-complete"; the narrower,
current scope is defined by `VERIFICATION_SCOPE.md` and the public README):

```text
0885b25 Establish feature-complete Genesis3D native Linux baseline
```

That baseline records the native runtime tree containing fixed-step physics pacing,
SDL-owned window and input handling, swept-hull BSP collision with stairs,
gravity and jumping, actor parsing and animation playback, runtime lightmaps,
PVS/translucent rendering, and the native OpenGL GPU cache.

The final build check completed successfully:

```text
[100%] Built target genesis3d_linux
```

## Repository hygiene

`.gitignore` now excludes:

- `build/`, `build-*`, `cmake-build-*`, and in-source CMake output;
- compiled objects, libraries, executables, and `genesis3d_linux`;
- `.act`, `.bsp`, `.gbsp`, purification copies, and binary scratch assets;
- `*.bak`, temporary files, profiling data, and Python caches; and
- all generated material below `Reports/evidence/`.

The original RealityFactory remote is retained under the provenance-oriented
name `upstream`:

```text
upstream https://github.com/RealityFactory/Genesis3D.git
```

## GitHub publication commands

The remote is already linked. Future updates require:

```bash
cd /path/to/Genesis3D-Linux
git push origin main
git push origin --tags
```

The commands used to link a fresh clone of this local history to the existing
public repository are:

```bash
git remote add origin https://github.com/Kadaken/Genesis3D-Linux.git
git push -u origin main
```

## Locations

- Native engine repository: the repository root
- Built local executable: `./genesis3d_linux` for the historical in-source
  build, or `./build/genesis3d_linux` for the documented out-of-tree build
- Project reports: `./Reports`
- Public Kadaken project page: <https://www.kadaken.com/ports#genesis3d>

The public project listing is live at:

```text
https://www.kadaken.com/ports#genesis3d
```

Kadaken site commit `d687489` adds the public-source listing and GitHub link.
It was deployed to the `kadaken` Cloudflare Pages project serving
`kadaken.com` and `www.kadaken.com`.
