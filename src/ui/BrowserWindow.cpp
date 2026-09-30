#include "BrowserWindow.h"
#include "ThemeManager.h"
#include "ShieldPopup.h"
#include "../app/AppConfig.h"
#include "../storage/StorageManager.h"
#include <commctrl.h>
#include <windowsx.h>
#include <sstream>
#include <algorithm>

namespace LiteBrowser {

namespace {
const wchar_t* MAIN_WINDOW_CLASS = L"LiteBrowser_MainWindow";
constexpr int TAB_BAR_HEIGHT = 38;
constexpr int TOOLBAR_HEIGHT = 46;
constexpr int TOP_CHROME_HEIGHT = TAB_BAR_HEIGHT + TOOLBAR_HEIGHT;
}

BrowserWindow::BrowserWindow() = default;

BrowserWindow::~BrowserWindow() = default;

bool BrowserWindow::Create() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hbrBackground = nullptr; // Double-buffered painting
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1320;
    int winH = 860;
    int winX = std::max<int>(0, (screenW - winW) / 2);
    int winY = std::max<int>(0, (screenH - winH) / 2);

    hWnd_ = CreateWindowExW(0, MAIN_WINDOW_CLASS, APP_NAME,
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            winX, winY, winW, winH,
                            nullptr, nullptr, GetModuleHandleW(nullptr), this);

    return hWnd_ != nullptr;
}

void BrowserWindow::Show(int nCmdShow) {
    if (hWnd_) {
        ShowWindow(hWnd_, nCmdShow);
        UpdateWindow(hWnd_);
    }
}

void BrowserWindow::OnCreate() {
    ThemeManager::GetInstance().ApplyThemeToWindow(hWnd_);

    tabManager_ = std::make_unique<TabManager>(hWnd_);
    findBar_ = std::make_unique<FindBar>(hWnd_);

    findBar_->SetFindCallback([this](const std::wstring& text, bool forward) {
        auto tab = tabManager_->GetActiveTab();
        if (tab) tab->FindInPage(text, forward);
    });

    findBar_->SetCloseCallback([this]() {
        auto tab = tabManager_->GetActiveTab();
        if (tab) tab->StopFind();
    });

    // Create Address Bar Edit Control
    hAddressEdit_ = CreateWindowExW(0, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                                    0, 0, 0, 0,
                                    hWnd_, (HMENU)201, GetModuleHandleW(nullptr), nullptr);

    HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    SendMessageW(hAddressEdit_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowSubclass(hAddressEdit_, AddressBarSubclassProc, 2, (DWORD_PTR)this);

    // Call UpdateLayout first so layout rects are initialized
    UpdateLayout();

    // Initialize WebView2 environments and open initial tab
    tabManager_->SetTabChangeCallback([this]() {
        UpdateControlsState();
        InvalidateRect(hWnd_, &rcTabBar_, FALSE);
        InvalidateRect(hWnd_, &rcToolbar_, FALSE);
    });

    tabManager_->InitializeEnvironments([this](bool success) {
        if (success) {
            tabManager_->CreateTab(URL_NEWTAB, false);
            UpdateLayout();
        } else {
            MessageBoxW(hWnd_,
                        L"Microsoft WebView2 Runtime is required to run LITE Browser.\nPlease install the WebView2 Evergreen Runtime.",
                        APP_NAME, MB_ICONERROR | MB_OK);
        }
    });
}

void BrowserWindow::OnDestroy() {
    PostQuitMessage(0);
}

void BrowserWindow::OnSize(int, int) {
    UpdateLayout();
}

void BrowserWindow::UpdateLayout() {
    if (!hWnd_) return;
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    rcTabBar_ = { 0, 0, w, TAB_BAR_HEIGHT };
    rcToolbar_ = { 0, TAB_BAR_HEIGHT, w, TOP_CHROME_HEIGHT };
    rcContent_ = { 0, TOP_CHROME_HEIGHT, w, h };

    // Toolbar Buttons Layout (centered vertically in TOOLBAR_HEIGHT=46, button height=32)
    int yMid = TAB_BAR_HEIGHT + (TOOLBAR_HEIGHT - 32) / 2;
    rcBtnBack_ = { 10, yMid, 42, yMid + 32 };
    rcBtnForward_ = { 46, yMid, 78, yMid + 32 };
    rcBtnReload_ = { 82, yMid, 114, yMid + 32 };

    rcBtnMenu_ = { w - 42, yMid, w - 10, yMid + 32 };
    rcBtnBookmark_ = { w - 78, yMid, w - 46, yMid + 32 };
    rcBtnReader_ = { w - 114, yMid, w - 82, yMid + 32 };
    rcBtnShield_ = { w - 192, yMid, w - 118, yMid + 32 };

    rcAddressBar_ = { 122, yMid, w - 200, yMid + 32 };

    if (hAddressEdit_) {
        SetWindowPos(hAddressEdit_, nullptr,
                     rcAddressBar_.left + 14, rcAddressBar_.top + 7,
                     (rcAddressBar_.right - rcAddressBar_.left) - 28, 18,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (tabManager_) {
        tabManager_->ResizeActiveTab(rcContent_);
    }

    if (findBar_ && findBar_->IsVisible()) {
        findBar_->Resize(rcContent_);
    }

    InvalidateRect(hWnd_, nullptr, FALSE);
}

void BrowserWindow::UpdateControlsState() {
    auto activeTab = tabManager_ ? tabManager_->GetActiveTab() : nullptr;
    if (activeTab && hAddressEdit_ && !addressBarFocused_) {
        SetWindowTextW(hAddressEdit_, activeTab->GetUrl().c_str());
        std::wstring windowTitle = activeTab->GetTitle().empty() ? APP_NAME : (activeTab->GetTitle() + L" - " + APP_NAME);
        SetWindowTextW(hWnd_, windowTitle.c_str());
    }
}

void BrowserWindow::OnPaint(HDC hdc) {
    RECT clientRc;
    GetClientRect(hWnd_, &clientRc);

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, clientRc.right, clientRc.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    ThemeColors colors = ThemeManager::GetInstance().GetColors();
    HBRUSH bgBrush = CreateSolidBrush(colors.background);
    FillRect(memDC, &clientRc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);

    HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

    // 1. Draw Tab Bar Background
    HBRUSH tabBarBgBrush = CreateSolidBrush(colors.background);
    FillRect(memDC, &rcTabBar_, tabBarBgBrush);
    DeleteObject(tabBarBgBrush);

    tabRects_.clear();
    tabCloseRects_.clear();
    int tabCount = tabManager_ ? tabManager_->GetTabCount() : 0;
    int activeIdx = tabManager_ ? tabManager_->GetActiveIndex() : -1;

    int curX = 12;
    int maxTabW = 200;
    int availW = clientRc.right - 60 - curX;
    int tabWidth = (tabCount > 0) ? std::min<int>(maxTabW, availW / tabCount) : maxTabW;
    if (tabWidth < 90) tabWidth = 90;

    for (int i = 0; i < tabCount; ++i) {
        auto tab = tabManager_->GetTab(i);
        if (!tab) continue;

        bool isActive = (i == activeIdx);
        RECT rcTab = { curX, 6, curX + tabWidth, TAB_BAR_HEIGHT };
        tabRects_.push_back(rcTab);

        if (isActive) {
            // Active tab seamlessly connects to toolbar
            HBRUSH tBrush = CreateSolidBrush(colors.surface);
            HPEN tPen = CreatePen(PS_SOLID, 1, colors.border);
            HPEN oldP = (HPEN)SelectObject(memDC, tPen);
            HBRUSH oldB = (HBRUSH)SelectObject(memDC, tBrush);

            // Rounded top corners
            RoundRect(memDC, rcTab.left, rcTab.top, rcTab.right, rcTab.bottom + 8, 8, 8);

            // Cover bottom line to fuse with toolbar
            RECT rcBottomCover = { rcTab.left + 1, rcTab.bottom - 1, rcTab.right - 1, rcTab.bottom + 1 };
            FillRect(memDC, &rcBottomCover, tBrush);

            SelectObject(memDC, oldP);
            SelectObject(memDC, oldB);
            DeleteObject(tPen);
            DeleteObject(tBrush);
        } else {
            // Inactive tab: subtle card with rounded top
            HBRUSH inactBrush = CreateSolidBrush(colors.background);
            HPEN inactPen = CreatePen(PS_SOLID, 1, colors.surfaceHover);
            HPEN oldP = (HPEN)SelectObject(memDC, inactPen);
            HBRUSH oldB = (HBRUSH)SelectObject(memDC, inactBrush);

            RoundRect(memDC, rcTab.left, rcTab.top + 2, rcTab.right, rcTab.bottom + 2, 6, 6);

            SelectObject(memDC, oldP);
            SelectObject(memDC, oldB);
            DeleteObject(inactPen);
            DeleteObject(inactBrush);
        }

        // Tab Title
        SetTextColor(memDC, isActive ? colors.text : colors.textSecondary);
        RECT rcTitle = { rcTab.left + 12, rcTab.top + (isActive ? 4 : 5), rcTab.right - 26, rcTab.bottom - 2 };
        std::wstring title = tab->GetTitle().empty() ? L"New Tab" : tab->GetTitle();
        if (tab->IsPrivate()) title = L"🔒 " + title;
        DrawTextW(memDC, title.c_str(), -1, &rcTitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        // Tab Close Button '✕'
        RECT rcClose = { rcTab.right - 24, rcTab.top + 6, rcTab.right - 8, rcTab.bottom - 6 };
        tabCloseRects_.push_back(rcClose);
        SetTextColor(memDC, colors.textSecondary);
        DrawTextW(memDC, L"×", -1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        curX += tabWidth + 4;
    }

    // New Tab button '+'
    rcBtnNewTab_ = { curX + 2, 7, curX + 32, TAB_BAR_HEIGHT - 3 };
    HBRUSH ntBrush = CreateSolidBrush(colors.surfaceHover);
    SelectObject(memDC, ntBrush);
    RoundRect(memDC, rcBtnNewTab_.left, rcBtnNewTab_.top, rcBtnNewTab_.right, rcBtnNewTab_.bottom, 6, 6);
    DeleteObject(ntBrush);
    SetTextColor(memDC, colors.text);
    DrawTextW(memDC, L"+", -1, &rcBtnNewTab_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 2. Draw Toolbar
    HBRUSH tbBrush = CreateSolidBrush(colors.surface);
    FillRect(memDC, &rcToolbar_, tbBrush);
    DeleteObject(tbBrush);

    // Toolbar Bottom Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, colors.border);
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, TOP_CHROME_HEIGHT - 1, nullptr);
    LineTo(memDC, clientRc.right, TOP_CHROME_HEIGHT - 1);

    // Nav Buttons
    auto activeTab = tabManager_ ? tabManager_->GetActiveTab() : nullptr;

    // Back button
    bool canBack = activeTab && activeTab->CanGoBack();
    SetTextColor(memDC, canBack ? colors.text : colors.textSecondary);
    DrawTextW(memDC, L"←", -1, &rcBtnBack_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Forward button
    bool canForward = activeTab && activeTab->CanGoForward();
    SetTextColor(memDC, canForward ? colors.text : colors.textSecondary);
    DrawTextW(memDC, L"→", -1, &rcBtnForward_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Reload / Stop button
    SetTextColor(memDC, colors.text);
    std::wstring reloadText = (activeTab && activeTab->IsLoading()) ? L"✕" : L"↻";
    DrawTextW(memDC, reloadText.c_str(), -1, &rcBtnReload_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Address Bar Pill (background & border)
    HBRUSH addrBrush = CreateSolidBrush(colors.background);
    HPEN addrPen = CreatePen(PS_SOLID, 1, addressBarFocused_ ? colors.accent : colors.border);
    HPEN oldAddrPen = (HPEN)SelectObject(memDC, addrPen);
    HBRUSH oldAddrBrush = (HBRUSH)SelectObject(memDC, addrBrush);

    RoundRect(memDC, rcAddressBar_.left, rcAddressBar_.top, rcAddressBar_.right, rcAddressBar_.bottom, 10, 10);

    SelectObject(memDC, oldAddrPen);
    SelectObject(memDC, oldAddrBrush);
    DeleteObject(addrPen);
    DeleteObject(addrBrush);

    // Privacy Shield Button
    int blockedTotal = activeTab ? (activeTab->GetBlockedAdsCount() + activeTab->GetBlockedTrackersCount()) : 0;
    if (blockedTotal > 0) {
        // Draw pill badge for shield
        HBRUSH badgeBrush = CreateSolidBrush(RGB(230, 245, 235));
        HPEN badgePen = CreatePen(PS_SOLID, 1, RGB(46, 170, 75));
        HPEN oldBPen = (HPEN)SelectObject(memDC, badgePen);
        HBRUSH oldBBrush = (HBRUSH)SelectObject(memDC, badgeBrush);

        RoundRect(memDC, rcBtnShield_.left, rcBtnShield_.top + 2, rcBtnShield_.right, rcBtnShield_.bottom - 2, 6, 6);

        SelectObject(memDC, oldBPen);
        SelectObject(memDC, oldBBrush);
        DeleteObject(badgePen);
        DeleteObject(badgeBrush);

        std::wstring shieldText = L"🛡 " + std::to_wstring(blockedTotal);
        SetTextColor(memDC, RGB(34, 139, 34));
        DrawTextW(memDC, shieldText.c_str(), -1, &rcBtnShield_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(memDC, colors.textSecondary);
        DrawTextW(memDC, L"🛡", -1, &rcBtnShield_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Reader Mode Button (only visible when available or active)
    if (activeTab && (activeTab->IsReaderAvailable() || activeTab->IsInReaderMode())) {
        SetTextColor(memDC, activeTab->IsInReaderMode() ? RGB(230, 130, 20) : colors.text);
        DrawTextW(memDC, L"📖", -1, &rcBtnReader_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Bookmark Button
    bool isBkmk = activeTab && StorageManager::GetInstance().IsBookmarked(activeTab->GetUrl());
    SetTextColor(memDC, isBkmk ? RGB(245, 166, 35) : colors.textSecondary);
    DrawTextW(memDC, isBkmk ? L"★" : L"☆", -1, &rcBtnBookmark_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Menu Button
    SetTextColor(memDC, colors.text);
    DrawTextW(memDC, L"☰", -1, &rcBtnMenu_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Copy to screen DC
    BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, hOldFont);
    DeleteObject(hFont);
    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

void BrowserWindow::OnLButtonDown(int x, int y) {
    POINT pt = { x, y };

    // Check Address Bar
    if (PtInRect(&rcAddressBar_, pt)) {
        if (hAddressEdit_) {
            SetFocus(hAddressEdit_);
            SendMessageW(hAddressEdit_, EM_SETSEL, 0, -1);
        }
        return;
    }

    // Check Content Area
    if (PtInRect(&rcContent_, pt)) {
        auto activeTab = tabManager_ ? tabManager_->GetActiveTab() : nullptr;
        if (activeTab) {
            activeTab->SetFocus();
        }
        return;
    }

    // Check Tabs
    int closeIdx = HitTestTabClose(x, y);
    if (closeIdx >= 0) {
        tabManager_->CloseTab(closeIdx);
        return;
    }

    int tabIdx = HitTestTab(x, y);
    if (tabIdx >= 0) {
        tabManager_->SelectTab(tabIdx);
        return;
    }

    if (HitTestNewTab(x, y)) {
        tabManager_->CreateTab(URL_NEWTAB, false);
        return;
    }

    // Check Toolbar
    auto activeTab = tabManager_ ? tabManager_->GetActiveTab() : nullptr;
    if (HitTestBack(x, y)) {
        if (activeTab) activeTab->GoBack();
        return;
    }
    if (HitTestForward(x, y)) {
        if (activeTab) activeTab->GoForward();
        return;
    }
    if (HitTestReload(x, y)) {
        if (activeTab) {
            if (activeTab->IsLoading()) activeTab->Stop();
            else activeTab->Reload();
        }
        return;
    }
    if (HitTestReader(x, y)) {
        if (activeTab) activeTab->ToggleReaderMode();
        return;
    }
    if (HitTestBookmark(x, y)) {
        if (activeTab) {
            std::wstring u = activeTab->GetUrl();
            if (StorageManager::GetInstance().IsBookmarked(u)) {
                auto bList = StorageManager::GetInstance().SearchBookmarks(u);
                for (const auto& b : bList) {
                    if (b.url == u) StorageManager::GetInstance().DeleteBookmark(b.id);
                }
            } else {
                StorageManager::GetInstance().AddBookmark(u, activeTab->GetTitle());
            }
            InvalidateRect(hWnd_, &rcToolbar_, FALSE);
        }
        return;
    }
    if (HitTestShield(x, y)) {
        if (activeTab) {
            std::wstring host = L"";
            std::wstring u = activeTab->GetUrl();
            size_t s = u.find(L"://");
            if (s != std::wstring::npos) {
                size_t e = u.find(L"/", s + 3);
                host = (e == std::wstring::npos) ? u.substr(s + 3) : u.substr(s + 3, e - s - 3);
            }

            POINT screenPt = pt;
            ClientToScreen(hWnd_, &screenPt);

            SiteSettings settings = StorageManager::GetInstance().GetSiteSettings(host);
            bool adsAllowed = (settings.adBlock == 0);
            bool trackersAllowed = (settings.trackerBlock == 0);

            ShieldPopup::GetInstance().Show(hWnd_, screenPt.x - 120, screenPt.y + 16,
                                           host,
                                           activeTab->GetBlockedAdsCount(),
                                           activeTab->GetBlockedTrackersCount(),
                                           adsAllowed, trackersAllowed,
                                           [host](bool allowAds, bool allowTrackers) {
                                               SiteSettings s;
                                               s.host = host;
                                               s.adBlock = allowAds ? 0 : 1;
                                               s.trackerBlock = allowTrackers ? 0 : 1;
                                               StorageManager::GetInstance().SetSiteSettings(s);
                                           });
        }
        return;
    }
    if (HitTestMenu(x, y)) {
        POINT screenPt = pt;
        ClientToScreen(hWnd_, &screenPt);
        ShowAppMenu(screenPt.x, screenPt.y);
        return;
    }
}

void BrowserWindow::OnRButtonDown(int x, int y) {
    int tabIdx = HitTestTab(x, y);
    if (tabIdx >= 0) {
        POINT pt = { x, y };
        ClientToScreen(hWnd_, &pt);
        ShowTabContextMenu(tabIdx, pt.x, pt.y);
    }
}

void BrowserWindow::OnMButtonDown(int x, int y) {
    int tabIdx = HitTestTab(x, y);
    if (tabIdx >= 0) {
        tabManager_->CloseTab(tabIdx);
    }
}

void BrowserWindow::OnMouseMove(int, int) {
}

int BrowserWindow::HitTestTab(int x, int y) {
    POINT pt = { x, y };
    for (size_t i = 0; i < tabRects_.size(); ++i) {
        if (PtInRect(&tabRects_[i], pt)) return (int)i;
    }
    return -1;
}

int BrowserWindow::HitTestTabClose(int x, int y) {
    POINT pt = { x, y };
    for (size_t i = 0; i < tabCloseRects_.size(); ++i) {
        if (PtInRect(&tabCloseRects_[i], pt)) return (int)i;
    }
    return -1;
}

bool BrowserWindow::HitTestNewTab(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnNewTab_, pt);
}

bool BrowserWindow::HitTestBack(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnBack_, pt);
}

bool BrowserWindow::HitTestForward(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnForward_, pt);
}

bool BrowserWindow::HitTestReload(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnReload_, pt);
}

bool BrowserWindow::HitTestReader(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnReader_, pt);
}

bool BrowserWindow::HitTestShield(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnShield_, pt);
}

bool BrowserWindow::HitTestBookmark(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnBookmark_, pt);
}

bool BrowserWindow::HitTestMenu(int x, int y) {
    POINT pt = { x, y };
    return PtInRect(&rcBtnMenu_, pt);
}

void BrowserWindow::NavigateAddressBar() {
    wchar_t buf[2048];
    GetWindowTextW(hAddressEdit_, buf, 2048);
    std::wstring input = buf;
    if (input.empty()) return;

    // Smart Navigation Interpreter
    std::wstring targetUrl;
    if (input.rfind(L"lite://", 0) == 0 ||
        input.rfind(L"http://", 0) == 0 ||
        input.rfind(L"https://", 0) == 0 ||
        input.rfind(L"file://", 0) == 0 ||
        input.rfind(L"about:", 0) == 0) {
        targetUrl = input;
    } else if (input.find(L'.') != std::wstring::npos && input.find(L' ') == std::wstring::npos) {
        targetUrl = L"https://" + input;
    } else {
        // Fallback to configured Search Engine
        targetUrl = L"https://www.google.com/search?q=" + input;
    }

    auto activeTab = tabManager_ ? tabManager_->GetActiveTab() : nullptr;
    if (activeTab) {
        activeTab->Navigate(targetUrl);
        activeTab->SetFocus();
    }
}

void BrowserWindow::ShowAppMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 301, L"New Tab\tCtrl+T");
    AppendMenuW(hMenu, MF_STRING, 302, L"New Private Tab\tCtrl+Shift+P");
    AppendMenuW(hMenu, MF_STRING, 303, L"Reopen Closed Tab\tCtrl+Shift+T");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 304, L"Bookmarks\tCtrl+B");
    AppendMenuW(hMenu, MF_STRING, 305, L"History\tCtrl+H");
    AppendMenuW(hMenu, MF_STRING, 306, L"Downloads\tCtrl+J");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 307, L"Privacy Inspector\tlite://privacy");
    AppendMenuW(hMenu, MF_STRING, 308, L"Settings\tlite://settings");
    AppendMenuW(hMenu, MF_STRING, 309, L"About LITE\tlite://about");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 310, L"Exit");

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hWnd_, nullptr);
    DestroyMenu(hMenu);
}

void BrowserWindow::ShowTabContextMenu(int tabIndex, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 401, L"Close Tab");
    AppendMenuW(hMenu, MF_STRING, 402, L"Close Other Tabs");
    AppendMenuW(hMenu, MF_STRING, 403, L"Close Tabs to the Right");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 404, L"Duplicate Tab");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hWnd_, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 401) tabManager_->CloseTab(tabIndex);
    else if (cmd == 402) tabManager_->CloseOtherTabs(tabIndex);
    else if (cmd == 403) tabManager_->CloseTabsToRight(tabIndex);
    else if (cmd == 404) tabManager_->DuplicateTab(tabIndex);
}

void BrowserWindow::OnCommand(int id) {
    switch (id) {
        case 301: tabManager_->CreateTab(URL_NEWTAB, false); break;
        case 302: tabManager_->CreateTab(URL_NEWTAB, true); break;
        case 303: tabManager_->ReopenClosedTab(); break;
        case 304: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_BOOKMARKS); break;
        case 305: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_HISTORY); break;
        case 306: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_DOWNLOADS); break;
        case 307: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_PRIVACY); break;
        case 308: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_SETTINGS); break;
        case 309: if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_ABOUT); break;
        case 310: DestroyWindow(hWnd_); break;
    }
}

bool BrowserWindow::HandleShortcut(WPARAM key, bool ctrl, bool shift, bool alt) {
    if (ctrl && !shift && !alt) {
        switch (key) {
            case 'T': tabManager_->CreateTab(URL_NEWTAB, false); return true;
            case 'W': tabManager_->CloseTab(tabManager_->GetActiveIndex()); return true;
            case 'R': if (auto t = tabManager_->GetActiveTab()) t->Reload(); return true;
            case 'L':
                if (hAddressEdit_) {
                    SetFocus(hAddressEdit_);
                    SendMessageW(hAddressEdit_, EM_SETSEL, 0, -1);
                }
                return true;
            case 'H': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_HISTORY); return true;
            case 'J': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_DOWNLOADS); return true;
            case 'B': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_BOOKMARKS); return true;
            case 'F': if (findBar_) findBar_->Show(); return true;
            case VK_OEM_PLUS:
            case VK_ADD: if (auto t = tabManager_->GetActiveTab()) t->ZoomIn(); return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT: if (auto t = tabManager_->GetActiveTab()) t->ZoomOut(); return true;
            case '0': if (auto t = tabManager_->GetActiveTab()) t->ResetZoom(); return true;
            case VK_TAB: tabManager_->NextTab(); return true;
        }
    } else if (ctrl && shift && !alt) {
        switch (key) {
            case 'T': tabManager_->ReopenClosedTab(); return true;
            case 'P': tabManager_->CreateTab(URL_NEWTAB, true); return true;
            case 'R': if (auto t = tabManager_->GetActiveTab()) t->Reload(true); return true;
            case VK_TAB: tabManager_->PreviousTab(); return true;
        }
    } else if (alt && !ctrl && !shift) {
        switch (key) {
            case VK_LEFT: if (auto t = tabManager_->GetActiveTab()) t->GoBack(); return true;
            case VK_RIGHT: if (auto t = tabManager_->GetActiveTab()) t->GoForward(); return true;
        }
    } else if (key == VK_F12) {
        if (auto t = tabManager_->GetActiveTab()) {
            t->OpenDevTools();
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK BrowserWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BrowserWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<BrowserWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_CREATE:
            if (self) self->OnCreate();
            return 0;
        case WM_SIZE:
            if (self) self->OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (self) self->OnPaint(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_SETFOCUS: {
            if (self && self->tabManager_) {
                auto activeTab = self->tabManager_->GetActiveTab();
                if (activeTab) {
                    activeTab->SetFocus();
                    return 0;
                }
            }
            break;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            if (self && (HWND)lParam == self->hAddressEdit_) {
                HDC hdcEdit = (HDC)wParam;
                ThemeColors colors = ThemeManager::GetInstance().GetColors();
                SetTextColor(hdcEdit, colors.text);
                SetBkColor(hdcEdit, colors.background);
                static HBRUSH hEditBgBrush = nullptr;
                if (hEditBgBrush) DeleteObject(hEditBgBrush);
                hEditBgBrush = CreateSolidBrush(colors.background);
                return (LRESULT)hEditBgBrush;
            }
            break;
        }
        case WM_LBUTTONDOWN:
            if (self) self->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_RBUTTONDOWN:
            if (self) self->OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MBUTTONDOWN:
            if (self) self->OnMButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_COMMAND:
            if (self) self->OnCommand(LOWORD(wParam));
            return 0;
        case WM_KEYDOWN: {
            if (self) {
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                if (self->HandleShortcut(wParam, ctrl, shift, alt)) {
                    return 0;
                }
            }
            break;
        }
        case WM_DESTROY:
            if (self) self->OnDestroy();
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK BrowserWindow::AddressBarSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    auto self = reinterpret_cast<BrowserWindow*>(dwRefData);
    switch (msg) {
        case WM_SETFOCUS:
            if (self) {
                self->addressBarFocused_ = true;
                InvalidateRect(self->hWnd_, &self->rcAddressBar_, FALSE);
            }
            break;
        case WM_KILLFOCUS:
            if (self) {
                self->addressBarFocused_ = false;
                self->UpdateControlsState();
                InvalidateRect(self->hWnd_, &self->rcAddressBar_, FALSE);
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                if (self) {
                    self->NavigateAddressBar();
                }
                return 0;
            } else if (wParam == VK_ESCAPE) {
                if (self) {
                    self->UpdateControlsState();
                    if (auto tab = self->tabManager_->GetActiveTab()) tab->SetFocus();
                }
                return 0;
            }
            break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

} // namespace LiteBrowser
