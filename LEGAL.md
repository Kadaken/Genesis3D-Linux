# Legal and provenance notes

This document records known licensing and provenance considerations. It is a
project compliance record, not legal advice and not a substitute for review by
qualified intellectual-property counsel.

## Controlling licence

Covered Genesis3D code is distributed under the Genesis3D Public License v1.01
in `g3dlicense.txt`. The project follows that written licence rather than
relying on informal descriptions of Genesis3D as "abandonware," "public
domain," or free of obligations.

The licence contains obligations beyond ordinary permissive open-source
licences, including publication of covered modifications, modification
documentation, required notices, and Genesis3D logo conditions. The project
therefore uses the term **source available**, not "public domain" or
"OSI-approved."

## Binary-release gate

Section 4.1 requires the original, unmodified Genesis3D animated logo to be the
first logo on startup of a product, demo, or application and requires the logo
on marketing materials. The current native validation executable does not yet
implement that animated startup sequence. No compiled public release should be
published from this branch until that requirement and every other executable
distribution obligation have been satisfied or qualified counsel has provided
different guidance.

## Provenance boundary

The Git lineage contains a Genesis3D 1.1 import and later community snapshots
labelled through 2009 before Kadaken's 2026 changes. It is inaccurate to
describe the present repository as an untouched 1999 tree or to assign all
copyright in the combined tree to one current entity. Existing source notices
identify Eclipse Entertainment, WildTangent, and other contributors; Kadaken
claims only its modifications.

## Inherited third-party header notices

The upstream history contained Microsoft resource/MFC header copies and an old
SGI `glext.h` carrying restrictive notices. They were unnecessary for the
native target and have been removed from the current branch. Because Git
history is inherited from a public upstream, prior revisions remain reachable.
Before any commercial distribution or high-profile release, counsel should
decide whether the repository should be rebuilt from a vetted source snapshot
or whether history rewriting is warranted.

## Third-party claims

Kadaken is not presently asserting knowledge of a specific active infringement
claim against the native modifications. If such a claim becomes known, section
3.5 of the Genesis3D Public License requires the `LEGAL` record to be updated
with enough information for recipients to contact the claimant.

## Assets and trademarks

No commercial game content is distributed. Compatible assets remain the user's
responsibility. Genesis3D and historical game names/logos are used only for
identification, attribution, licence compliance, and historical discussion.
All trademarks and copyrights remain with their respective owners. No
affiliation, certification, or endorsement is claimed.

## Recommended professional review

Before distributing binaries, accepting payment specifically for the port, or
shipping a commercial product based on it, obtain an attorney's review of:

1. the current ownership and enforceability of the Genesis3D Public License;
2. the required animated-logo implementation and marketing use;
3. the inherited community contribution chain;
4. historical third-party files still visible in Git history; and
5. the licence boundary between Covered Code and a separate game or Larger Work.
