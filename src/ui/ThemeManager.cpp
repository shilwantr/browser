#include "ThemeManager.h"
#include <dwmapi.h>

namespace LiteBrowser {

ThemeManager& ThemeManager::GetInstance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    currentMode_ = ThemeMode::System;
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::SetThemeMode(ThemeMode mode) {
    currentMode_ = mode;
}

bool ThemeManager::DetectSystemDarkTheme() const {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD sz = sizeof(val);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val == 0);
        }
        RegCloseKey(hKey);
    }
    return true; // default dark
}

bool ThemeManager::IsDarkTheme() const {
    if (currentMode_ == ThemeMode::Dark) return true;
    if (currentMode_ == ThemeMode::Light) return false;
    return DetectSystemDarkTheme();
}

ThemeColors ThemeManager::GetColors() const {
    return IsDarkTheme() ? GetDarkThemeColors() : GetLightThemeColors();
}

void ThemeManager::ApplyThemeToWindow(HWND hWnd) {
    if (!hWnd) return;
    BOOL dark = IsDarkTheme() ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 10 20H1+ and Windows 11)
    DwmSetWindowAttribute(hWnd, 20, &dark, sizeof(dark));
    // DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 = 19
    DwmSetWindowAttribute(hWnd, 19, &dark, sizeof(dark));
}

} // namespace LiteBrowser
