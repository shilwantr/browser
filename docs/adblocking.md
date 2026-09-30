# Ad & Tracker Blocking Engine

LITE Browser includes a high-performance network-level request interceptor (`AdBlockEngine`) that blocks intrusive advertisements, analytics scripts, telemetry beacons, and fingerprinting infrastructure.

## Blocking Architecture

Unlike browser extensions that inject DOM-hiding CSS after an ad has already downloaded and executed, LITE Browser hooks directly into WebView2's resource request pipeline:

```text
Web Page Request
       │
       ▼
ICoreWebView2::WebResourceRequested
       │
       ▼
AdBlockEngine::ShouldBlock()
       ├── Fast Domain Hash Lookup: O(1)
       ├── Domain Suffix / Hierarchy Traversal
       ├── Resource Type Filtering (Script, Image, Subdocument)
       ├── First-Party vs Third-Party Detection
       └── Site Allowlist Check
       │
       ├─── [Matched & Blocked] ───► Respond 204 No Content (Local)
       │                             (Zero network traffic emitted)
       │
       └─── [Allowed] ─────────────► Forward request to network
```

## Supported Rule Syntax

`RuleParser` supports standard EasyList and EasyPrivacy rule syntax:

| Syntax | Example | Behavior |
|---|---|---|
| Domain Anchor | `||doubleclick.net^` | Blocks `doubleclick.net` and all subdomains (`ad.doubleclick.net`). |
| Exception Rule | `@@||safe.example.com^` | Unblocks/allowlists resources from `safe.example.com`. |
| Third-Party Modifier | `||tracker.com^$third-party` | Blocks only when requested from a different origin than the document host. |
| Resource Types | `||ads.com^$script,image` | Targets specific resource types. |
| Substring / Path | `/pagead/ads.js` | Matches specific request path patterns. |

## Preloaded Infrastructure

The browser comes with built-in blocking lists for over 100 top advertising networks, telemetry endpoints, and trackers, including:
- DoubleClick, Google AdSense, Criteo, Taboola, Outbrain, Rubicon, AppNexus.
- Google Analytics, Tag Manager, Hotjar, Segment, Mixpanel, CrazyEgg, FullStory, Clarity.
- Facebook Graph tracking pixels, Twitter/TikTok/Snapchat trackers.
- Windows/Microsoft telemetry and telemetry collection domains.

## Live Inspection

Users can visit `lite://privacy` to inspect a live log of recent network requests, blocked status, matched rules, and active filter counts.
