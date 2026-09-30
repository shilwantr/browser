# Privacy Specification & Threat Model

LITE Browser was built to eliminate telemetry, background network tracking, and surveillance capitalism at the browser shell layer.

## Core Privacy Principles

1. **Zero Telemetry**:
   - The browser executable does not contain any analytics SDKs (Google Analytics, Firebase, Sentry, Mixpanel, Segment, etc.).
   - No URLs, queries, bookmarks, or system fingerprints are sent to any remote server.
2. **No Accounts or Logins**:
   - The browser operates completely locally without requiring user sign-in or cloud identity synchronization.
3. **Local-Only Search Autocomplete**:
   - The address bar autocomplete suggests only local history and bookmarks stored on the machine.
4. **Isolated Private Browsing**:
   - Private tabs use an isolated WebView2 environment with an ephemeral data profile (`%LOCALAPPDATA%\LiteBrowser\PrivateData`).
   - Cookies, session data, and cache created in a private tab are deleted upon closing the private session and never touch the main profile database.
5. **No Background Services**:
   - When all browser windows are closed, all browser processes terminate immediately. No background updater daemon, telemetry reporter, or service remains resident in memory.

## Data Storage Locations

All user data is stored strictly on the local machine under `%LOCALAPPDATA%\LiteBrowser\`:

- `browser.db`: SQLite database storing bookmarks, folders, history, site settings, and preferences.
- `UserData/`: WebView2 persistent profile for normal browsing.
- `PrivateData/`: Ephemeral isolated profile for private browsing.

Users can clear browsing data at any time via `lite://history` ("Clear All History") or through the Windows uninstaller.
