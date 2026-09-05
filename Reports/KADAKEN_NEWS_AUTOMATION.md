# Kadaken headline-news automation

Date: 2026-09-04

## Current state

The scrolling bar carries the three current Kadaken updates:

1. Agent Workbench Native 4.17.0 is available.
2. Genesis3D now has a native Linux runtime.
3. CodeOp 4.0 Beta 34 is open for testing.

The whole set is duplicated once in the HTML only to make the animation loop
smoothly. The copy is hidden from assistive technology and is not a second set
of news.

## Single source of truth

The ordered news list lives in `news.json` in the Kadaken website repository.
Remove stale items, add timely ones, and arrange them in display order there.

Agent Workbench and CodeOp use version placeholders. The website fills those
from each application's live update manifest—the same sources that supply the
download versions. When CodeOp's manifest moves from `4.0.0-beta+34` to
`4.0.0-beta+35`, the public wording automatically changes from “CodeOp 4.0
Beta 34” to “CodeOp 4.0 Beta 35.” Agent Workbench updates the same way. The
edge cache may take approximately 60 seconds to refresh.

The raw HTML uses version-free fallback wording. If a manifest is temporarily
unavailable, the page does not advertise an obsolete version number.

## What to tell Codex or Claude

For a normal Agent Workbench or CodeOp release:

> Publish the application release and update its live release manifest. Verify
> Kadaken `/versions` and the live news bar show the new version. Keep the other
> current news items intact.

To change which projects or announcements appear:

> Update Kadaken news in `news.json`, retain all still-current items, remove
> only stale items, deploy the website, and verify `/versions` and the live
> homepage.

Editing `news.json` requires a website deployment. A routine version change
does not require a website deployment after its application manifest is live.

## CodeOp version wording

CodeOp's established Flutter/updater form is `4.0.0-beta+34`:

- `4.0.0-beta` is the version name;
- `34` is the build number; and
- “CodeOp 4.0 Beta 34” is the intended reader-facing form.

At the time of this check, `codeop-flutter/lib/main.dart`, the Rust package, and
the live updater identify Beta 34. The checked-in Flutter `pubspec.yaml` and
Rust lockfile still contain Beta 30 metadata. Those files should be synchronized
by the next CodeOp release pass; they were not edited here to avoid colliding
with concurrent CodeOp work.

## Verification

- Live-manifest rendering of all three news items: PASS.
- Simulated Agent Workbench 4.18.0 update: PASS.
- Simulated CodeOp Beta 35 update: PASS.
- Desktop headless visual check: PASS.
- JavaScript syntax checks: PASS.
- HTML validation errors: 0.
- Production `/versions` response: PASS.
- Production homepage rendering: PASS.

Website commit: `e6c3fef`.
Cloudflare production deployment: `c59682a9`.
