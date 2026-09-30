#pragma once

#include <windows.h>
#include <string>
#include <functional>

namespace LiteBrowser {

class FindBar {
public:
    FindBar(HWND hParent);
    ~FindBar();

    void Show();
    void Hide();
    bool IsVisible() const;

    void SetFindCallback(std::function<void(const std::wstring& text, bool forward)> callback) {
        onFind_ = callback;
    }
    void SetCloseCallback(std::function<void()> callback) {
        onClose_ = callback;
    }

    void Resize(const RECT& bounds);

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    HWND hParent_ = nullptr;
    HWND hWnd_ = nullptr;
    HWND hEdit_ = nullptr;
    HWND btnPrev_ = nullptr;
    HWND btnNext_ = nullptr;
    HWND btnClose_ = nullptr;

    std::function<void(const std::wstring&, bool)> onFind_;
    std::function<void()> onClose_;
};

} // namespace LiteBrowser
