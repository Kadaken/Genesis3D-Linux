# Kadaken headline-news automation

Date: 2026-09-04

## Current state

The scrolling bar contains one current news item:

> CodeOp 4.0 Beta 34 is now open for testing

The duplicate seen in the HTML is only an `aria-hidden` animation clone used to
make the same sentence scroll continuously. It is not a second news item.

## Single source of truth

Edit this file in the Kadaken website repository:

```text
news.json
```

Current contents:

```json
{
  "text": "CodeOp {{codeop.major_minor}} Beta {{codeop.beta}} is now open for testing",
  "href": "#codeop"
}
```

The placeholders are filled from CodeOp's live update manifest—the same source
that supplies the website's download-button versions. When the manifest changes
from `4.0.0-beta+34` to `4.0.0-beta+35`, the bar changes from Beta 34 to Beta 35
automatically. The edge cache may take up to approximately 60 seconds to refresh.
No Kadaken website edit or deployment is required for a normal CodeOp beta.

If the manifest is temporarily unavailable, the page uses the generic fallback
“CodeOp 4.0 beta testing is open” rather than displaying an obsolete number.

## What to tell Codex or Claude

For a normal CodeOp release:

> Publish the new CodeOp release and update its live release manifest. Verify
> that Kadaken `/versions` and the homepage news bar show the new beta number.

For a completely different headline:

> Update Kadaken headline news in `news.json`. Keep exactly one news item,
> update its link, deploy the website, and verify `/versions` and the live
> homepage.

For a non-CodeOp item, replace the `text` with ordinary text containing no
placeholders and change `href` to its page or section. Changing `news.json`
requires a website deployment.

## Implementation and verification

- `functions/versions.js` reads `news.json` and the CodeOp manifest.
- `functions/_middleware.js` inserts the current headline before HTML is sent.
- `fill-versions.js` provides a browser-side refresh as a second path.
- The HTML contains one meaningful headline plus one assistive-technology-hidden
  copy for continuous animation.
- Beta 34 live-manifest rendering: PASS.
- Simulated manifest update to Beta 35 without an HTML edit: PASS.
- Desktop headless visual check: PASS.
- HTML validation errors: 0.
- Production `/versions` response: PASS.
- Production homepage rendering: PASS.

Website commits: `b696972`, `55c9e0e`.
