#pragma once

#include <string>
#include <windows.h>

namespace LiteBrowser {

// Application Information
constexpr const wchar_t* APP_NAME = L"LITE Browser";
constexpr const wchar_t* APP_VERSION = L"0.1.0";
constexpr const wchar_t* APP_PUBLISHER = L"LITE Browser Project";
constexpr const wchar_t* APP_WEBSITE = L"https://litebrowser.org";
constexpr const wchar_t* APP_USER_AGENT_SUFFIX = L"LiteBrowser/0.1.0";

// Internal Schemes and URLs
constexpr const wchar_t* SCHEME_INTERNAL = L"lite";
constexpr const wchar_t* URL_NEWTAB = L"lite://newtab";
constexpr const wchar_t* URL_SETTINGS = L"lite://settings";
constexpr const wchar_t* URL_HISTORY = L"lite://history";
constexpr const wchar_t* URL_BOOKMARKS = L"lite://bookmarks";
constexpr const wchar_t* URL_DOWNLOADS = L"lite://downloads";
constexpr const wchar_t* URL_PRIVACY = L"lite://privacy";
constexpr const wchar_t* URL_ABOUT = L"lite://about";

// Theme tokens (Monochrome editorial design)
enum class ThemeMode {
    System = 0,
    Light = 1,
    Dark = 2
};

struct ThemeColors {
    COLORREF background;
    COLORREF surface;
    COLORREF surfaceHover;
    COLORREF text;
    COLORREF textSecondary;
    COLORREF border;
    COLORREF accent;
};

inline ThemeColors GetLightThemeColors() {
    ThemeColors c{};
    c.background = RGB(255, 255, 255);
    c.surface = RGB(247, 247, 248);
    c.surfaceHover = RGB(238, 239, 241);
    c.text = RGB(20, 20, 20);
    c.textSecondary = RGB(110, 115, 122);
    c.border = RGB(225, 227, 230);
    c.accent = RGB(15, 15, 15);
    return c;
}

inline ThemeColors GetDarkThemeColors() {
    ThemeColors c{};
    c.background = RGB(24, 24, 26);
    c.surface = RGB(32, 33, 36);
    c.surfaceHover = RGB(44, 46, 50);
    c.text = RGB(240, 242, 245);
    c.textSecondary = RGB(154, 160, 166);
    c.border = RGB(54, 57, 63);
    c.accent = RGB(255, 255, 255);
    return c;
}

// Search Engine Defaults
struct SearchEngine {
    std::wstring name;
    std::wstring searchUrl; // Template containing %s
    std::wstring suggestUrl;
};

inline const SearchEngine DEFAULT_SEARCH_ENGINES[] = {
    { L"Google", L"https://www.google.com/search?q=%s", L"https://suggestqueries.google.com/complete/search?client=chrome&q=%s" },
    { L"DuckDuckGo", L"https://duckduckgo.com/?q=%s", L"" },
    { L"Bing", L"https://www.bing.com/search?q=%s", L"" },
    { L"Brave Search", L"https://search.brave.com/search?q=%s", L"" }
};

} // namespace LiteBrowser
