# Reader Mode Engine

Reader Mode is LITE Browser's hero feature, transforming cluttered web articles into clean, distraction-free reading material.

## Article Detection & Confidence Scoring

When a web page completes loading, `ReaderModeEngine` runs an in-page scoring algorithm to assess whether the page contains an article candidate:

$$\text{Confidence Score} = S_{\text{semantic}} + S_{\text{metadata}} + S_{\text{paragraphs}} + S_{\text{length}} - P_{\text{links}}$$

- **Semantic Tags**: `+35` for `<article>`, `+20` for `<main>`.
- **Metadata**: `+30` for `meta[property="og:type"] == "article"`, `+35` for JSON-LD `@type: Article`.
- **Paragraphs**: `+20` for 3+ dense paragraphs.
- **Word Count**: `+25` for 250+ words, `+20` for 600+ words.
- **Link Penalty**: Penalizes high link-to-text ratios to reject menus and directory pages.

When confidence passes the threshold ($\ge 50$ points and 200+ words), the `📖` Reader button appears in the toolbar.

## Content Extraction & Aggressive Sanitization

Upon clicking Reader Mode:
1. The DOM tree is cloned locally.
2. Unrelated elements are removed: navigation, footers, sidebars, social share bars, newsletter signups, comments, and inline advertisements.
3. **C++ Sanitization Pass**:
   - Strips `<script>`, `<iframe>`, `<object>`, `<embed>`, `<form>`, `<input>`, `<button>`, `<svg>`, `<canvas>`.
   - Strips all inline event handlers (`onclick`, `onload`, `onerror`, `onmouseover`, etc.).
   - Strips `javascript:` pseudo-protocol URLs.
   - Allows only clean semantic elements (`h1`–`h6`, `p`, `blockquote`, `img`, `figure`, `figcaption`, `ul`, `ol`, `li`, `pre`, `code`, `table`, `a`).
4. Strict Content-Security-Policy is enforced on the reader document:
   ```http
   Content-Security-Policy: default-src 'none'; img-src https: http: data:; style-src 'unsafe-inline'; font-src data:;
   ```

## Typography & Themes

The Reader UI supports:
- **Themes**:
  - Warm (Sepia: `#fbf7ee` background, `#2c2523` text)
  - Light (Crisp White: `#ffffff` background, `#1a1a1a` text)
  - Dark (OLED Black: `#19191a` background, `#e3e3e3` text)
- **Typography**:
  - Serif (Georgia)
  - Sans-Serif (Segoe UI Variable)
  - Monospace (Consolas)
- **Controls**:
  - Estimated reading time badge (`words / 200` words per minute).
  - One-click Exit Reader button restoring the original web view immediately.
