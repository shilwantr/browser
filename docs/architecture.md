# Architecture Documentation

LITE Browser is designed from the ground up as a native Win32 application leveraging the installed Microsoft WebView2 Evergreen Runtime. It achieves an exceptionally small binary size and near-instant startup time by avoiding heavyweight application frameworks (Electron, CEF, Qt) while strictly managing process memory.

## Architectural Overview

```text
┌────────────────────────────────────────────────────────┐
│                      Win32 UI Layer                    │
│  BrowserWindow  │  TabBarView  │  Toolbar  │  FindBar  │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                    Browser Core                        │
│   TabManager   │   Tab   │   InternalPages   │  Theme  │
└──────┬────────────────────┬────────────────────┬───────┘
       │                    │                    │
┌──────▼──────┐      ┌──────▼──────┐      ┌──────▼───────┐
│   AdBlock   │      │ Reader Mode │      │   Storage    │
│   Engine    │      │   Engine    │      │ SQLite 3 DB  │
└──────┬──────┘      └──────┬──────┘      └──────────────┘
       │                    │
┌──────▼────────────────────▼────────────────────────────┐
│          Microsoft WebView2 Win32 / C++ COM            │
│       Environment  │  Controller  │  CoreWebView2      │
└────────────────────────────────────────────────────────┘
```

## Core Modules

### 1. Application Layer (`src/app/`)
- `AppConfig.h`: Centralizes all branding constants (`APP_NAME`, `APP_VERSION`, `APP_PUBLISHER`, `APP_WEBSITE`) and design tokens so brand identity is maintained in a single place.
- `ComUtils.h`: Implements lightweight, zero-overhead RAII COM smart pointers (`ComPtr<T>`) and type-safe callback bridges (`MakeCallback<IInterface>`). This eliminates dependencies on heavy WRL/WIL runtime libraries, cutting binary size significantly.
- `main.cpp`: Win32 entry point with single-threaded apartment COM initialization, common controls setup, and standard event message dispatch.

### 2. Tab Management & Lifecycle (`src/tabs/`)
- `Tab`: Represents a discrete tab. Owns its `ICoreWebView2Controller`, navigation state (`canGoBack`, `canGoForward`, `isLoading`), blocked statistics, zoom level, and Reader Mode status.
- `TabManager`: Manages tab collections, tab ordering, active tab switching, tab pinning, duplication, and the 20-entry recently closed tabs stack (`Ctrl+Shift+T`).
- **Resource Management**: When a tab is closed, its controller and webview COM instances are immediately released. WebView2 environments are reused between standard tabs while isolated environments are spawned for private tabs.

### 3. Native UI (`src/ui/`)
- `BrowserWindow`: Main window with double-buffered GDI rendering for zero-flicker UI chrome.
- `ShieldPopup`: Lightweight native popup displaying site privacy stats and allowing ad/tracker toggling per host.
- `FindBar`: Non-intrusive in-page search toolbar with instant highlight and match navigation.
- `ThemeManager`: Automatically detects Windows Dark Mode and synchronizes with DWM (`DwmSetWindowAttribute`) for modern window frames.

### 4. Privacy & Network Ad Blocking (`src/adblock/`)
- `RuleParser`: Parses EasyList / Adblock Plus syntax rules.
- `AdBlockEngine`: Employs O(1) hash sets for domain anchors (`||domain.com^`) and fast substring matchers for resource rules. Intercepts web resource requests via `add_WebResourceRequested` and returns 204 No Content responses for blocked resources before packets touch external networks.

### 5. Reader Mode Engine (`src/reader/`)
- `ReaderModeEngine`: Evaluates web page content using semantic signals (`<article>`, `<main>`, paragraph density, link ratio). Extracts clean article text, strips scripts and tracking pixels, sanitizes HTML against XSS, and renders a local presentation document with warm, light, and dark typography themes.

### 6. Persistence & Storage (`src/storage/`)
- `StorageManager`: Thread-safe SQLite 3 database (`browser.db`) configured with Write-Ahead Logging (WAL) and normal synchronous mode for fast disk operations without UI thread stalls.
