# Genesis3D Native Linux Git Main Baseline

Date: 2026-09-03

## Result

The existing repository history was preserved and audited rather than replaced
with a new `.git` directory. The production branch is now named `main`, with no
upstream tracking branch configured until the new GitHub repository exists.

Baseline commit:

```text
0885b25 Establish feature-complete Genesis3D native Linux baseline
```

That baseline records the production tree containing fixed-step physics pacing,
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

After creating an empty GitHub repository, replace the two bracketed values and
run exactly:

```bash
cd /home/user/Genesis3D_Project
git remote add origin git@github.com:<GITHUB_USERNAME>/<REPOSITORY_NAME>.git
git push -u origin main
git push origin --tags
```

For an HTTPS remote instead of SSH, use:

```bash
git remote add origin https://github.com/<GITHUB_USERNAME>/<REPOSITORY_NAME>.git
```

Do not use GitHub's “initialize with README” option when creating the remote;
the local repository already contains the authoritative history.

## Locations

- Native engine repository: `/home/user/Genesis3D_Project`
- Built local executable: `/home/user/Genesis3D_Project/genesis3d_linux`
- Project reports: `/home/user/Genesis3D_Project/Reports`
- Local Kadaken website project discovered during the audit:
  `/home/user/Projects/Open Projects/kadaken.com`

No Kadaken website files were changed in this task. Publishing or adding a
Genesis3D page/download to `www.kadaken.com` should be handled as a separate
website-scoped change after the GitHub repository URL is known.
