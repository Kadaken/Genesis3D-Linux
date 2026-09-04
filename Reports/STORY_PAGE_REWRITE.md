# Genesis3D story-page rewrite

Date: 2026-09-04

## Outcome

The Kadaken “Read the story” page was rewritten as a short, general-reader
history rather than an engine implementation report. The published article is
now 647 words, down from approximately 1,657 words.

The story focuses on:

- Eclipse Entertainment's goal of making 3D creation more approachable;
- the kinds of games for which Genesis3D is remembered;
- WildTangent's 1999 acquisition and web-focused plan;
- the engine's gradual decline rather than an invented single death date; and
- Kadaken's non-commercial Linux preservation project.

Detailed renderer, BSP, collision, timing, lightmap, and source-file discussion
was removed from the public story. That material remains in the repository's
technical reports.

## Accuracy basis

- RealityFactory's preserved Genesis3D repository and history:
  <https://github.com/RealityFactory/Genesis3D>
- Contemporary reporting on WildTangent's acquisition of Eclipse technology:
  <https://www.internetnews.com/developer/wildtangent-acquires-eclipse/>
- Historical Genesis3D game catalogue:
  <https://www.mobygames.com/group/7425/3d-engine-genesis3d/>

The article deliberately says Genesis3D faded gradually. Available evidence
supports the acquisition, later 1999 release, subsequent games, and later source
snapshots, but does not establish one official date on which the engine “died.”

## Publication and verification

```text
Website commit: 59d8350
Cloudflare production deployment: 8d2f3d18
Live route: https://www.kadaken.com/genesis3d
Desktop headless render: PASS
390 x 844 mobile headless render: PASS
Local W3C validation errors: 0
Live W3C validation errors: 0
Live content check: PASS
Website worktree after deployment: clean
```
