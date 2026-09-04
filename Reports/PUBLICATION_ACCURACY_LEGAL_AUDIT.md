# Genesis3D Linux public publication audit

Date: 2026-09-04

## Outcome

Kadaken's Genesis3D publication has been reorganized and rewritten around
claims that the public source history and recorded tests support. The engine is
presented as a **source-available native Linux runtime**, not as a Kadaken game,
an untouched 1999 release, a universally tested SDK, or an entirely original
Kadaken product.

This review is a practical source, licence, provenance, and publication audit.
It reduces avoidable exposure but cannot guarantee legality or replace advice
from a qualified intellectual-property attorney.

## Requested work checklist

- [x] Audit the public Kadaken home page.
- [x] Audit the Genesis3D story article.
- [x] Audit the community invitation.
- [x] Audit the public GitHub repository and its presentation.
- [x] Separate Ports from Kadaken's applications.
- [x] Remove Genesis3D from the Games page.
- [x] Add a first-class Ports page.
- [x] Add a clickable moving Latest-updates bar to the home page.
- [x] Make the updates bar pause for interaction and respect reduced motion.
- [x] Add documented games historically associated with Genesis3D.
- [x] Correct historical, technical, ownership, and completion overclaims.
- [x] Preserve the original Genesis3D licence text unchanged.
- [x] Add licence discovery, attribution, modification, legal, security, and
  contribution documents.
- [x] Add required licence notices to Kadaken's new native source/build tools.
- [x] Remove unused inherited headers carrying restrictive Microsoft/SGI
  notices from the current branch.
- [x] Confirm no commercial game assets or compiled binaries are tracked.
- [x] Remove personal absolute filesystem paths from public documentation.
- [x] Run a fresh engine build without opening a window or capturing input.
- [x] Validate changed HTML, internal links, desktop layout, and mobile layout.

## Public information architecture

The Kadaken home page now contains Kadaken applications only. Its primary
navigation links to four distinct destinations:

1. Apps on the home page;
2. Ports at `/ports`;
3. Games at `/games`; and
4. small utilities in the App Drawer.

Genesis3D appears under Ports. It no longer appears on the Games page. The
separate Ports notice states that the ports are not Kadaken products and are
governed by their original licences, not Kadaken's app terms.

The home page now has a Latest-updates rail whose Genesis3D announcement links
directly to `/ports#genesis3d`. It uses CSS animation rather than the obsolete
`marquee` element, pauses on pointer hover or keyboard focus, hides its duplicate
content from assistive technology, and becomes a manually scrollable static row
when reduced motion is requested.

## Claims removed or corrected

### Historical lineage

Removed the claim that nothing came after Genesis3D 1.1. The inherited Git
history identifies a 1.1 import followed by inherited source snapshots labelled
through 2009. The publication now describes the repository as a lineage of 1.1,
later inherited revisions, and Kadaken's 2026 port.

Removed the unsupported absolute claim that this is the first native Linux
port. Earlier Linux port attempts exist, and the available evidence does not
justify an absolute worldwide first.

Removed the claim that the current tree is exactly where it was left in 1999.
It is published source rather than a decompilation, but it includes later
inherited revisions.

Removed the ambiguous label "community-maintained" from the lineage
description. The evidence establishes later snapshots in the inherited
RealityFactory repository, but that phrase could incorrectly imply outside
community participation in Kadaken's Linux port. The publication now states
explicitly that Kadaken developed the 2026 port with assistance from Gemini and
Codex and does not claim outside community contributors to the port.

### Copyright and ownership

Removed the claim that WildTangent presently holds all copyright in the source
as published. The combined tree contains notices for Eclipse Entertainment,
WildTangent, and community contributors. Kadaken now claims only its identified
modifications and expressly disclaims affiliation or endorsement.

### Native renderer

Removed the contradictory claim that the old OpenGL renderer was not rewritten.
The build excludes the historical driver implementations. The accurate
description is that a new SDL2/OpenGL frontend loads retained engine world and
actor structures and submits BSP materials, lightmaps, and posed actors.

### Completion and testing

Removed broad descriptions such as a complete engine/SDK and full universal
support. Claims are now scoped to one verified Fedora KDE AMD/Mesa system and
the tests recorded in `Reports/VERIFICATION_SCOPE.md`. The public page states
that this is an engine runtime and validation sandbox, not a game or complete
replacement for every historical tool and subsystem.

### Community invitation

The invitation no longer implies that a fresh clone includes everything needed
to play. It says source is available, commercial game assets are excluded, and
runtime testing requires compatible assets the tester is legally entitled to
use. Contributors are directed to the controlling licence and must certify that
submitted work is original or properly licensed. Commercial game data, leaked
source, proprietary SDK code, and unauthorized assets are explicitly rejected.

## Historical game examples

The article now names *Extreme PaintBrawl 2*, *Catechumen*, *Ominous Horizons:
A Paladin's Calling*, and *Dragon's Lair 3D: Return to the Lair* as documented
or historically catalogued Genesis3D titles. The paragraph makes clear that the
games, names, and assets are not included in or licensed by this port.

Reference links are included directly in the article. The broader catalogue is
also available from
[MobyGames](https://www.mobygames.com/group/7425/3d-engine-genesis3d/).

## Repository safeguards added

- `LICENSE` — conventional top-level discovery notice pointing to the complete
  controlling text.
- `g3dlicense.txt` — retained unchanged from upstream.
- `NOTICE.md` — lineage, source availability, asset boundary, system libraries,
  and non-endorsement.
- `MODIFICATIONS.md` — dated description and prominent derivation statement for
  section 3.4.
- `LEGAL` and `LEGAL.md` — the exact named legal record requested by the
  licence plus detailed provenance and release risks.
- `CONTRIBUTING.md` — contributor grant, clean-submission certification, asset
  prohibition, evidence expectations, and safe testing rules.
- `SECURITY.md` — private vulnerability-reporting route and supported scope.

New Kadaken engine files now carry the Genesis3D Public License notice and
identify Kadaken as the 2026 native Linux contributor. Existing source notices
remain intact.

## Third-party header cleanup

The current branch no longer contains the unused copies of:

- `Afxres.h`;
- `Winres.h`;
- `Winresrc.h`; or
- the historical `Engine/Drivers/OpenGl/glext.h`.

Those files carried restrictive Microsoft or SGI notices and are not used by
the native target. They remain reachable in inherited upstream Git history.
`LEGAL.md` discloses that fact rather than pretending the historical chain is
fully cleared.

## Verification evidence

### Publication records

```text
Engine audited implementation changeset: 9d9795093f3b1bcbc6bd2f05cb82bb0dc0e25d0f
Website repository main: 6859ddc7f504a3bc1e953adf88ecda6bdfad1983
Cloudflare production deployment: 5c210b77-6d67-4746-8d0d-3e951a36a7f0
Cloudflare deployment source: 6859ddc
GitHub visibility: PUBLIC
GitHub binary releases: 0
```

The local and remote `main` hashes matched after publication. The GitHub
description and homepage now use the scoped runtime language and point to the
dedicated Kadaken Ports entry.

### Engine

Fresh out-of-tree release configuration and build:

```text
GNU C/C++ 16.2.1
Genesis3D native implementation modules: 70
[100%] Built target genesis3d_linux
```

The build was compiled only. The executable was not launched, no window was
opened, and no keyboard or mouse capture occurred.

Additional checks:

```text
Tracked .act/.bsp/.gbsp/audio/executable/library assets: 0
g3dlicense.txt differs from upstream: no
Personal absolute home-directory paths in public tree: none
Obvious secret patterns in tracked text: none found
Python asset/include tools: syntax PASS
git diff --check: PASS
Legacy toxic title/vocabulary in public source tree: none found
```

### Website

```text
Internal static links and same-page fragments: PASS
W3C validator errors — index.html: 0
W3C validator errors — ports.html: 0
W3C validator errors — games.html: 0
W3C validator errors — genesis3d.html: 0
W3C validator errors — terms.html: 0
Desktop headless rendering: PASS
390 × 844 mobile headless rendering: PASS
Live production routes `/`, `/ports`, `/games`, `/genesis3d`, `/terms`: HTTP 200
Live fingerprinted stylesheet referenced by production HTML: HTTP 200
Live GitHub README, LEGAL, CONTRIBUTING, and roadmap links: HTTP 200
Live W3C validator errors across the five publication pages: 0
```

## Controlling sources consulted

- [Genesis3D Public License v1.01 in the upstream tree](https://github.com/RealityFactory/Genesis3D/blob/master/g3dlicense.txt)
- [RealityFactory Genesis3D source and history](https://github.com/RealityFactory/Genesis3D)
- [Contemporary report of WildTangent acquiring Eclipse technology](https://www.internetnews.com/developer/wildtangent-acquires-eclipse/)
- [Current genesis3d.com historical overview](https://www.genesis3d.com/)
- [GitHub guidance on repository licensing](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)

The current genesis3d.com page informally characterizes the legacy engine as
free of licensing obligations. This project does **not** rely on that statement;
it continues to follow the written Genesis3D Public License v1.01 distributed
with the source unless qualified counsel establishes a different controlling
grant.

## Residual legal/release gates

### 1. Do not publish a compiled release yet

The Genesis3D Public License requires the original unmodified animated
Genesis3D logo to be the first logo at startup of a distributed product, demo,
or application and requires the logo on marketing. Kadaken displays the
original badge on the project marketing pages, but the native validation
executable does not yet implement the animated startup sequence. The public
project is therefore source-only and offers no binary release.

### 2. Inherited history needs counsel before a major commercial release

Removing restrictive third-party headers from the current branch does not erase
them from inherited Git history. Rewriting public history has not been done
because it is destructive and would sever or complicate provenance. Counsel
should decide whether a future commercial release should use a newly vetted
history or whether other remediation is appropriate.

### 3. No audit can guarantee title, trademark, or licence enforceability

The publication avoids ownership and endorsement claims and uses historical
names for identification. A qualified attorney should still review current
rights ownership, trademark posture, the logo requirement, the contributor
chain, and the boundary between Covered Code and a future separate game before
money is accepted specifically for the port or a compiled product is shipped.

### 4. Neutral redistributable fixtures remain desirable

The source builds without commercial assets, but a new contributor cannot fully
exercise runtime paths without providing compatible data. Original,
redistributable BSP and ACT fixtures would make community testing more useful
without weakening the asset boundary.

## Safe public description

> Genesis3D Native Linux is a source-available native 64-bit Linux runtime for
> the Genesis3D 1.x engine lineage. It combines preserved
> Eclipse/WildTangent and later inherited engine code with Kadaken's 2026
> SDL2/OpenGL Linux frontend. Its documented BSP rendering, actor animation,
> input, collision, lightmap, timing, and teardown paths have been tested on one
> Fedora AMD/Mesa system. It is an engine runtime, not a game or a complete port
> of every historical SDK tool. No commercial game assets or compiled release
> are distributed.
