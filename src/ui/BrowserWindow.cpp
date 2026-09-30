#include "BrowserWindow.h"
#include "ThemeManager.h"
#include "ShieldPopup.h"
#include "../app/AppConfig.h"
#include "../storage/StorageManager.h"
#include <commctrl.h>
#include <windowsx.h>
#include <sstream>

namespace LiteBrowser {

namespace {
const wchar_t* MAIN_WINDOW_CLASS = L"LiteBrowser_MainWindow";
constexpr int TAB_BAR_HEIGHT = 34;
constexpr int TOOLBAR_HEIGHT = 40;
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

    hWnd_ = CreateWindowExW(0, MAIN_WINDOW_CLASS, APP_NAME,
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
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

    HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(hAddressEdit_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowSubclass(hAddressEdit_, AddressBarSubclassProc, 2, (DWORD_PTR)this);

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

void BrowserWindow::OnSize(int width, int height) {
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

    // Toolbar Buttons Layout
    int yMid = TAB_BAR_HEIGHT + (TOOLBAR_HEIGHT - 28) / 2;
    rcBtnBack_ = { 10, yMid, 38, yMid + 28 };
    rcBtnForward_ = { 42, yMid, 70, yMid + 28 };
    rcBtnReload_ = { 74, yMid, 102, yMid + 28 };

    rcBtnMenu_ = { w - 38, yMid, w - 10, yMid + 28 };
    rcBtnBookmark_ = { w - 70, yMid, w - 42, yMid + 28 };
    rcBtnReader_ = { w - 102, yMid, w - 74, yMid + 28 };
    rcBtnShield_ = { w - 165, yMid, w - 106, yMid + 28 };

    rcAddressBar_ = { 110, yMid, w - 175, yMid + 28 };

    if (hAddressEdit_) {
        SetWindowPos(hAddressEdit_, nullptr,
                     rcAddressBar_.left + 8, rcAddressBar_.top + 5,
                     (rcAddressBar_.right - rcAddressBar_.left) - 16, 18,
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

    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

    // 1. Draw Tab Bar
    tabRects_.clear();
    tabCloseRects_.clear();
    int tabCount = tabManager_ ? tabManager_->GetTabCount() : 0;
    int activeIdx = tabManager_ ? tabManager_->GetActiveIndex() : -1;

    int curX = 8;
    int tabWidth = (tabCount > 0) ? std::min<int>(180, (clientRc.right - 60) / tabCount) : 180;
    if (tabWidth < 80) tabWidth = 80;

    for (int i = 0; i < tabCount; ++i) {
        auto tab = tabManager_->GetTab(i);
        if (!tab) continue;

        RECT rcTab = { curX, 4, curX + tabWidth, TAB_BAR_HEIGHT };
        tabRects_.push_back(rcTab);

        bool isActive = (i == activeIdx);
        COLORREF tabBg = isActive ? colors.surface : colors.background;
        HBRUSH tBrush = CreateSolidBrush(tabBg);
        HPEN tPen = CreatePen(PS_SOLID, 1, isActive ? colors.border : colors.background);
        HPEN oldP = (HPEN)SelectObject(memDC, tPen);
        HBRUSH oldB = (HBRUSH)SelectObject(memDC, tBrush);

        RoundRect(memDC, rcTab.left, rcTab.top, rcTab.right, rcTab.bottom + 4, 6, 6);

        SelectObject(memDC, oldP);
        SelectObject(memDC, oldB);
        DeleteObject(tPen);
        DeleteObject(tBrush);

        // Tab Title
        SetTextColor(memDC, isActive ? colors.text : colors.textSecondary);
        RECT rcTitle = { rcTab.left + 10, rcTab.top + 7, rcTab.right - 24, rcTab.bottom };
        std::wstring title = tab->GetTitle().empty() ? L"New Tab" : tab->GetTitle();
        if (tab->IsPrivate()) title = L"[Private] " + title;
        DrawTextW(memDC, title.c_str(), -1, &rcTitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        // Close button '✕'
        RECT rcClose = { rcTab.right - 22, rcTab.top + 7, rcTab.right - 6, rcTab.bottom - 7 };
        tabCloseRects_.push_back(rcClose);
        DrawTextW(memDC, L"✕", -1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        curX += tabWidth + 2;
    }

    // New Tab button '+'
    rcBtnNewTab_ = { curX + 2, 6, curX + 28, TAB_BAR_HEIGHT - 4 };
    SetTextColor(memDC, colors.textSecondary);
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
    SetTextColor(memDC, (activeTab && activeTab->CanGoBack()) ? colors.text : colors.textSecondary);
    DrawTextW(memDC, L"←", -1, &rcBtnBack_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDC, (activeTab && activeTab->CanGoForward()) ? colors.text : colors.textSecondary);
    DrawTextW(memDC, L"→", -1, &rcBtnForward_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDC, colors.text);
    std::wstring reloadText = (activeTab && activeTab->IsLoading()) ? L"✕" : L"↻";
    DrawTextW(memDC, reloadText.c_str(), -1, &rcBtnReload_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Address Bar background & border
    HBRUSH addrBrush = CreateSolidBrush(colors.background);
    SelectObject(memDC, addrBrush);
    RoundRect(memDC, rcAddressBar_.left, rcAddressBar_.top, rcAddressBar_.right, rcAddressBar_.bottom, 6, 6);
    DeleteObject(addrBrush);

    // Privacy Shield Button
    int blockedTotal = activeTab ? (activeTab->GetBlockedAdsCount() + activeTab->GetBlockedTrackersCount()) : 0;
    std::wstring shieldText = (blockedTotal > 0) ? (L"🛡 " + std::to_wstring(blockedTotal)) : L"🛡";
    SetTextColor(memDC, (blockedTotal > 0) ? RGB(60, 180, 75) : colors.textSecondary);
    DrawTextW(memDC, shieldText.c_str(), -1, &rcBtnShield_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Reader Mode Button (only visible when available or currently active)
    if (activeTab && (activeTab->IsReaderAvailable() || activeTab->IsInReaderMode())) {
        SetTextColor(memDC, activeTab->IsInReaderMode() ? RGB(230, 140, 20) : colors.text);
        DrawTextW(memDC, L"📖", -1, &rcBtnReader_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Bookmark Button
    bool isBkmk = activeTab && StorageManager::GetInstance().IsBookmarked(activeTab->GetUrl());
    SetTextColor(memDC, isBkmk ? RGB(235, 180, 30) : colors.textSecondary);
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

void BrowserWindow::OnMouseMove(int x, int y) {
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

void BrowserWindow::HandleShortcut(WPARAM key, bool ctrl, bool shift, bool alt) {
    if (ctrl && !shift && !alt) {
        switch (key) {
            case 'T': tabManager_->CreateTab(URL_NEWTAB, false); break;
            case 'W': tabManager_->CloseTab(tabManager_->GetActiveIndex()); break;
            case 'R': if (auto t = tabManager_->GetActiveTab()) t->Reload(); break;
            case 'L': SetFocus(hAddressEdit_); SendMessageW(hAddressEdit_, EM_SETSEL, 0, -1); break;
            case 'H': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_HISTORY); break;
            case 'J': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_DOWNLOADS); break;
            case 'B': if (auto t = tabManager_->GetActiveTab()) t->Navigate(URL_BOOKMARKS); break;
            case 'F': if (findBar_) findBar_->Show(); break;
            case VK_OEM_PLUS:
            case VK_ADD: if (auto t = tabManager_->GetActiveTab()) t->ZoomIn(); break;
            case VK_OEM_MINUS:
            case VK_SUBTRACT: if (auto t = tabManager_->GetActiveTab()) t->ZoomOut(); break;
            case '0': if (auto t = tabManager_->GetActiveTab()) t->ResetZoom(); break;
            case VK_TAB: tabManager_->NextTab(); break;
        }
    } else if (ctrl && shift && !alt) {
        switch (key) {
            case 'T': tabManager_->ReopenClosedTab(); break;
            case 'P': tabManager_->CreateTab(URL_NEWTAB, true); break;
            case 'R': if (auto t = tabManager_->GetActiveTab()) t->Reload(true); break;
            case VK_TAB: tabManager_->PreviousTab(); break;
        }
    } else if (alt && !ctrl && !shift) {
        switch (key) {
            case VK_LEFT: if (auto t = tabManager_->GetActiveTab()) t->GoBack(); break;
            case VK_RIGHT: if (auto t = tabManager_->GetActiveTab()) t->GoForward(); break;
        }
    } else if (key == VK_F12) {
        if (auto t = tabManager_->GetActiveTab()) t->OpenDevTools();
    }
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
                self->HandleShortcut(wParam, ctrl, shift, alt);
            }
            return 0;
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
            if (self) self->addressBarFocused_ = true;
            break;
        case WM_KILLFOCUS:
            if (self) {
                self->addressBarFocused_ = false;
                self->UpdateControlsState();
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                if (self) self->NavigateAddressBar();
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
