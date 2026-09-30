#include "FindBar.h"
#include "ThemeManager.h"
#include <commctrl.h>

namespace LiteBrowser {

namespace {
const wchar_t* FINDBAR_CLASS = L"LiteBrowser_FindBar";
}

FindBar::FindBar(HWND hParent) : hParent_(hParent) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = FINDBAR_CLASS;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    RegisterClassExW(&wc);

    hWnd_ = CreateWindowExW(WS_EX_CONTROLPARENT, FINDBAR_CLASS, L"",
                            WS_CHILD | WS_CLIPSIBLINGS,
                            0, 0, 320, 36,
                            hParent_, nullptr, GetModuleHandleW(nullptr), this);

    hEdit_ = CreateWindowExW(0, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                             8, 6, 200, 24,
                             hWnd_, (HMENU)101, GetModuleHandleW(nullptr), nullptr);

    btnPrev_ = CreateWindowExW(0, L"BUTTON", L"↑",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               214, 6, 28, 24,
                               hWnd_, (HMENU)102, GetModuleHandleW(nullptr), nullptr);

    btnNext_ = CreateWindowExW(0, L"BUTTON", L"↓",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               246, 6, 28, 24,
                               hWnd_, (HMENU)103, GetModuleHandleW(nullptr), nullptr);

    btnClose_ = CreateWindowExW(0, L"BUTTON", L"✕",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                278, 6, 28, 24,
                                hWnd_, (HMENU)104, GetModuleHandleW(nullptr), nullptr);

    SetWindowSubclass(hEdit_, EditSubclassProc, 1, (DWORD_PTR)this);
}

FindBar::~FindBar() {
    if (hWnd_) DestroyWindow(hWnd_);
}

void FindBar::Show() {
    if (hWnd_) {
        ShowWindow(hWnd_, SW_SHOW);
        SetFocus(hEdit_);
        SendMessageW(hEdit_, EM_SETSEL, 0, -1);
    }
}

void FindBar::Hide() {
    if (hWnd_ && IsWindowVisible(hWnd_)) {
        ShowWindow(hWnd_, SW_HIDE);
        if (onClose_) onClose_();
    }
}

bool FindBar::IsVisible() const {
    return hWnd_ && IsWindowVisible(hWnd_);
}

void FindBar::Resize(const RECT& bounds) {
    if (!hWnd_) return;
    int width = 320;
    int height = 36;
    int x = bounds.right - width - 20;
    int y = bounds.top + 8;
    SetWindowPos(hWnd_, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
}

LRESULT CALLBACK FindBar::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FindBar* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<FindBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<FindBar*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            ThemeColors colors = ThemeManager::GetInstance().GetColors();
            HBRUSH brush = CreateSolidBrush(colors.surface);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
            HPEN borderPen = CreatePen(PS_SOLID, 1, colors.border);
            HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(borderPen);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_COMMAND: {
            if (!self) break;
            int id = LOWORD(wParam);
            if (id == 102) { // Prev
                wchar_t buf[256];
                GetWindowTextW(self->hEdit_, buf, 256);
                if (self->onFind_) self->onFind_(buf, false);
            } else if (id == 103) { // Next
                wchar_t buf[256];
                GetWindowTextW(self->hEdit_, buf, 256);
                if (self->onFind_) self->onFind_(buf, true);
            } else if (id == 104) { // Close
                self->Hide();
            } else if (id == 101 && HIWORD(wParam) == EN_CHANGE) {
                wchar_t buf[256];
                GetWindowTextW(self->hEdit_, buf, 256);
                if (self->onFind_) self->onFind_(buf, true);
            }
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK FindBar::EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    auto self = reinterpret_cast<FindBar*>(dwRefData);
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            if (self) self->Hide();
            return 0;
        } else if (wParam == VK_RETURN) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            wchar_t buf[256];
            GetWindowTextW(hWnd, buf, 256);
            if (self && self->onFind_) self->onFind_(buf, !shift);
            return 0;
        }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

} // namespace LiteBrowser
