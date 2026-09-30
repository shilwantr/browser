#pragma once

#include <windows.h>
#include <memory>
#include <vector>
#include "../tabs/TabManager.h"
#include "FindBar.h"

namespace LiteBrowser {

class BrowserWindow {
public:
    BrowserWindow();
    ~BrowserWindow();

    bool Create();
    void Show(int nCmdShow);
    HWND GetHwnd() const { return hWnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK AddressBarSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnPaint(HDC hdc);
    void OnLButtonDown(int x, int y);
    void OnRButtonDown(int x, int y);
    void OnMButtonDown(int x, int y);
    void OnMouseMove(int x, int y);
    void OnCommand(int id);
    bool HandleShortcut(WPARAM key, bool ctrl, bool shift, bool alt);

    void UpdateLayout();
    void UpdateControlsState();
    void NavigateAddressBar();
    void ShowAppMenu(int x, int y);
    void ShowTabContextMenu(int tabIndex, int x, int y);

    // Hit testing for custom UI
    int HitTestTab(int x, int y);
    int HitTestTabClose(int x, int y);
    bool HitTestNewTab(int x, int y);
    bool HitTestBack(int x, int y);
    bool HitTestForward(int x, int y);
    bool HitTestReload(int x, int y);
    bool HitTestReader(int x, int y);
    bool HitTestShield(int x, int y);
    bool HitTestBookmark(int x, int y);
    bool HitTestMenu(int x, int y);

    HWND hWnd_ = nullptr;
    HWND hAddressEdit_ = nullptr;

    std::unique_ptr<TabManager> tabManager_;
    std::unique_ptr<FindBar> findBar_;

    // Layout Rectangles
    RECT rcTabBar_{};
    RECT rcToolbar_{};
    RECT rcContent_{};
    RECT rcBtnBack_{};
    RECT rcBtnForward_{};
    RECT rcBtnReload_{};
    RECT rcAddressBar_{};
    RECT rcBtnReader_{};
    RECT rcBtnShield_{};
    RECT rcBtnBookmark_{};
    RECT rcBtnMenu_{};
    RECT rcBtnNewTab_{};

    std::vector<RECT> tabRects_;
    std::vector<RECT> tabCloseRects_;

    // State
    bool isDraggingTab_ = false;
    int dragTabIndex_ = -1;
    int hoveredTabIndex_ = -1;
    bool addressBarFocused_ = false;
    bool suppressNextMouseUp_ = false;
    HBRUSH hAddressEditBrush_ = nullptr;
    COLORREF lastBrushColor_ = 0;
};

} // namespace LiteBrowser
