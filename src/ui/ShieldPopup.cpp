#include "ShieldPopup.h"
#include "ThemeManager.h"
#include <sstream>

namespace LiteBrowser {

namespace {
const wchar_t* SHIELD_POPUP_CLASS = L"LiteBrowser_ShieldPopup";
}

ShieldPopup& ShieldPopup::GetInstance() {
    static ShieldPopup instance;
    return instance;
}

ShieldPopup::ShieldPopup() = default;

ShieldPopup::~ShieldPopup() {
    if (hWnd_) DestroyWindow(hWnd_);
}

void ShieldPopup::Show(HWND hParent, int x, int y,
                       const std::wstring& host,
                       int adsBlocked,
                       int trackersBlocked,
                       bool adsAllowed,
                       bool trackersAllowed,
                       std::function<void(bool, bool)> onToggle) {
    hParent_ = hParent;
    host_ = host;
    adsBlocked_ = adsBlocked;
    trackersBlocked_ = trackersBlocked;
    adsAllowed_ = adsAllowed;
    trackersAllowed_ = trackersAllowed;
    onToggle_ = onToggle;

    if (!hWnd_) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = SHIELD_POPUP_CLASS;
        wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
        wc.style = CS_DROPSHADOW;
        RegisterClassExW(&wc);

        hWnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                SHIELD_POPUP_CLASS, L"Privacy Shield",
                                WS_POPUP | WS_BORDER,
                                x, y, 260, 200,
                                hParent, nullptr, GetModuleHandleW(nullptr), this);
    }

    SetWindowPos(hWnd_, HWND_TOPMOST, x, y, 260, 200, SWP_SHOWWINDOW);
    InvalidateRect(hWnd_, nullptr, TRUE);
    UpdateWindow(hWnd_);
}

void ShieldPopup::Hide() {
    if (hWnd_ && IsWindowVisible(hWnd_)) {
        ShowWindow(hWnd_, SW_HIDE);
    }
}

bool ShieldPopup::IsVisible() const {
    return hWnd_ && IsWindowVisible(hWnd_);
}

LRESULT CALLBACK ShieldPopup::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ShieldPopup* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<ShieldPopup*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ShieldPopup*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (self) self->OnPaint(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_KILLFOCUS: {
            if (self) self->Hide();
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (self) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                if (PtInRect(&self->btnToggleAds_, pt)) {
                    self->adsAllowed_ = !self->adsAllowed_;
                    if (self->onToggle_) self->onToggle_(self->adsAllowed_, self->trackersAllowed_);
                    InvalidateRect(hWnd, nullptr, TRUE);
                } else if (PtInRect(&self->btnToggleTrackers_, pt)) {
                    self->trackersAllowed_ = !self->trackersAllowed_;
                    if (self->onToggle_) self->onToggle_(self->adsAllowed_, self->trackersAllowed_);
                    InvalidateRect(hWnd, nullptr, TRUE);
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ShieldPopup::OnPaint(HDC hdc) {
    RECT rc;
    GetClientRect(hWnd_, &rc);

    ThemeColors colors = ThemeManager::GetInstance().GetColors();
    HBRUSH bgBrush = CreateSolidBrush(colors.surface);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colors.text);

    HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Header
    RECT rcTitle = { 16, 14, rc.right - 16, 34 };
    DrawTextW(hdc, L"Site Privacy", -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

    // Hostname
    HFONT hFontSub = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontSub);
    SetTextColor(hdc, colors.textSecondary);
    RECT rcHost = { 16, 34, rc.right - 16, 50 };
    DrawTextW(hdc, host_.empty() ? L"Current page" : host_.c_str(), -1, &rcHost, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Stats
    SetTextColor(hdc, colors.text);
    std::wstring adsText = L"Ads blocked: " + std::to_wstring(adsBlocked_);
    RECT rcAds = { 16, 58, rc.right - 16, 76 };
    DrawTextW(hdc, adsText.c_str(), -1, &rcAds, DT_LEFT | DT_SINGLELINE);

    std::wstring trText = L"Trackers blocked: " + std::to_wstring(trackersBlocked_);
    RECT rcTr = { 16, 78, rc.right - 16, 96 };
    DrawTextW(hdc, trText.c_str(), -1, &rcTr, DT_LEFT | DT_SINGLELINE);

    // Buttons
    btnToggleAds_ = { 16, 108, rc.right - 16, 140 };
    btnToggleTrackers_ = { 16, 148, rc.right - 16, 180 };

    HBRUSH btnBrush = CreateSolidBrush(colors.surfaceHover);
    HPEN btnPen = CreatePen(PS_SOLID, 1, colors.border);
    HPEN oldPen = (HPEN)SelectObject(hdc, btnPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, btnBrush);

    RoundRect(hdc, btnToggleAds_.left, btnToggleAds_.top, btnToggleAds_.right, btnToggleAds_.bottom, 4, 4);
    RoundRect(hdc, btnToggleTrackers_.left, btnToggleTrackers_.top, btnToggleTrackers_.right, btnToggleTrackers_.bottom, 4, 4);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(btnPen);
    DeleteObject(btnBrush);

    std::wstring adsBtnText = adsAllowed_ ? L"Block ads on this site" : L"Allow ads on this site";
    std::wstring trBtnText = trackersAllowed_ ? L"Block trackers on this site" : L"Allow trackers on this site";

    DrawTextW(hdc, adsBtnText.c_str(), -1, &btnToggleAds_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, trBtnText.c_str(), -1, &btnToggleTrackers_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hFontSub);
}

} // namespace LiteBrowser
