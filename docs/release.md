# Release & Distribution Notes

## Release Checklist

- [x] All 36 automated unit tests passing (`LiteBrowserTests.exe`)
- [x] Native Release binary compiled with `-O3 -static` and stripped of debug symbols
- [x] Package size verified below 8 MB budget (1.80 MB EXE, 964 KB portable zip)
- [x] WebView2 Evergreen Runtime detection verified
- [x] Windows Dark/Light mode theme switching verified
- [x] AdBlock and Tracker blocking verified at the network level
- [x] Reader Mode extraction and XSS sanitization verified
- [x] Bookmarks, history, and closed tab persistence verified
- [x] Chrome and Edge bookmark import verified
- [x] Native installer (`LiteBrowser-Setup-x64.exe`) and uninstaller (`Uninstall.exe`) verified

## Deliverable Artifacts

1. **`dist/LiteBrowser-Portable-x64.zip`**: Self-contained portable edition. Run anywhere without administrator rights.
2. **`dist/LiteBrowser-Setup-x64.exe`**: Native Windows installer that validates the WebView2 runtime, installs to `%LOCALAPPDATA%\Programs\LiteBrowser`, creates Start Menu shortcuts, and registers uninstall entries.
3. **`build/release/LiteBrowser.exe`**: Direct standalone application binary.
4. **`build/release/LiteBrowserTests.exe`**: Verification and regression test suite executable.
