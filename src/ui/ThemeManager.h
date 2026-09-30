#pragma once

#include <windows.h>
#include "../app/AppConfig.h"

namespace LiteBrowser {

class ThemeManager {
public:
    static ThemeManager& GetInstance();

    ThemeManager();
    ~ThemeManager();

    void SetThemeMode(ThemeMode mode);
    ThemeMode GetThemeMode() const { return currentMode_; }

    bool IsDarkTheme() const;
    ThemeColors GetColors() const;

    void ApplyThemeToWindow(HWND hWnd);

private:
    bool DetectSystemDarkTheme() const;

    ThemeMode currentMode_ = ThemeMode::System;
};

} // namespace LiteBrowser
