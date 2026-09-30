# LITE Browser

An ultra-lightweight, minimal Windows desktop web browser designed for people who just want the web without the garbage.

LITE Browser pairs native Win32 performance with Microsoft's Evergreen WebView2 runtime. It starts in milliseconds, uses virtually zero idle resources, blocks advertisements and trackers at the network level, and features a distraction-free, local Reader Mode.

---

## Highlights

- **Extremely Small Footprint**: Application executable is **1.80 MB**, and portable package is under **1 MB** (964 KB).
- **Fast Startup**: ~419 ms warm launch, ~873 ms cold launch.
- **Zero Telemetry**: No Google Analytics, no Microsoft telemetry, no background pings, no accounts, and no data leaves your machine.
- **Network-Level Ad & Tracker Blocking**: Built-in `AdBlockEngine` intercepts web resource requests before they leave the browser, supporting EasyList and EasyPrivacy filter rules.
- **Distraction-Free Reader Mode**: Automatic article detection with semantic scoring, clean local extraction, aggressive XSS/script sanitization, and reader customization (Light, Warm, Dark themes, typography, and width controls).
- **Real Multi-Tab Browsing**: Tab dragging, pinning, tab duplication, closed tab history (`Ctrl+Shift+T`), and isolated Private Browsing sessions.
- **Local Persistence via SQLite**: Structured bookmarks with folders, browsing history with search and cleanup, per-site zoom and blocking overrides.
- **Chrome & Edge Bookmark Importer**: Seamless 1-click import of existing bookmarks from local Chrome and Edge profile databases.

---

## Measured Performance & Package Footprint

| Metric | Measured Value | Target |
|---|---|---|
| **Application Executable Size** (`LiteBrowser.exe`) | **1.80 MB** (1,890,816 bytes) | 3–8 MB |
| **Total Installed Binary Footprint** | **1.99 MB** (2,085,728 bytes) | 3–8 MB |
| **Setup Installer Size** (`LiteBrowser-Setup-x64.exe`) | **224 KB** (229,888 bytes) | < 5 MB |
| **Portable Package Size** (`LiteBrowser-Portable-x64.zip`) | **964 KB** (987,376 bytes) | < 5 MB |
| **Cold Startup Time** | **873 ms** | < 1500 ms |
| **Warm Startup Time** | **419 ms** | < 800 ms |
| **Native Process Working Set** (Idle) | **29.94 MB** | < 50 MB |
| **Native Process Private Bytes** | **5.94 MB** | < 15 MB |

*Note: Microsoft WebView2 Runtime provides the Chromium rendering engine and is deployed separately via Windows Evergreen updates, ensuring the browser executable stays ultra-compact.*

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl + T` | Open new tab |
| `Ctrl + W` | Close active tab |
| `Ctrl + Shift + T` | Reopen last closed tab |
| `Ctrl + Tab` | Switch to next tab |
| `Ctrl + Shift + Tab` | Switch to previous tab |
| `Ctrl + L` | Focus address bar |
| `Ctrl + R` | Reload current page |
| `Ctrl + Shift + R` | Hard reload (bypass cache) |
| `Alt + Left` | Navigate back |
| `Alt + Right` | Navigate forward |
| `Ctrl + D` | Toggle bookmark for current page |
| `Ctrl + H` | Open history (`lite://history`) |
| `Ctrl + B` | Open bookmarks (`lite://bookmarks`) |
| `Ctrl + J` | Open downloads (`lite://downloads`) |
| `Ctrl + Shift + P` | Open new Private tab |
| `Ctrl + F` | Find in page |
| `Ctrl + +` / `Ctrl + -` | Zoom in / Zoom out |
| `Ctrl + 0` | Reset zoom to 100% |
| `F12` | Toggle Developer Tools / Inspector |

---

## Internal Schemes (`lite://`)

- `lite://newtab` - Minimalist start page with top sites, bookmarks, and search.
- `lite://settings` - Clean settings for search engines, blocking, and themes.
- `lite://history` - Searchable browsing history with time-based deletion.
- `lite://bookmarks` - Bookmark management and browser import tools.
- `lite://downloads` - Active and completed download manager.
- `lite://privacy` - Real-time request monitor and ad/tracker rule inspector.
- `lite://about` - Version, licenses, and architecture details.

---

## Building from Source

### Prerequisites
- Windows 10/11 x64
- CMake 3.20 or newer
- Ninja build system
- Clang (LLVM-MinGW) or Visual Studio MSVC 2022+

### Build Steps
```powershell
# Clone the repository
git clone https://github.com/litebrowser/litebrowser.git
cd "litebrowser"

# Configure with CMake (Release build)
cmake -B build/release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang

# Compile
cmake --build build/release

# Run automated test suite
.\build\release\LiteBrowserTests.exe

# Launch browser
.\build\release\LiteBrowser.exe
```

---

## Third-Party Components & Attribution

- **Microsoft WebView2 SDK**: Copyright (c) Microsoft Corporation. Used under the MIT License.
- **SQLite**: Dedicated to the Public Domain by D. Richard Hipp.
- **EasyList & EasyPrivacy**: Filter syntax and matching methodologies adhere to GPLv3 / CC BY-SA 3.0 attribution guidelines.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
