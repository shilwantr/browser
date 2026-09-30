#pragma once

#include <windows.h>
#include <string>
#include <functional>

namespace LiteBrowser {

class ShieldPopup {
public:
    static ShieldPopup& GetInstance();

    ShieldPopup();
    ~ShieldPopup();

    void Show(HWND hParent, int x, int y,
              const std::wstring& host,
              int adsBlocked,
              int trackersBlocked,
              bool adsAllowed,
              bool trackersAllowed,
              std::function<void(bool allowAds, bool allowTrackers)> onToggleCallback);

    void Hide();
    bool IsVisible() const;

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);

    HWND hWnd_ = nullptr;
    HWND hParent_ = nullptr;
    std::wstring host_;
    int adsBlocked_ = 0;
    int trackersBlocked_ = 0;
    bool adsAllowed_ = false;
    bool trackersAllowed_ = false;
    std::function<void(bool, bool)> onToggle_;

    RECT btnToggleAds_{};
    RECT btnToggleTrackers_{};
};

} // namespace LiteBrowser
