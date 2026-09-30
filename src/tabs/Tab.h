#pragma once

#include <string>
#include <memory>
#include <functional>
#include <windows.h>
#include "WebView2.h"
#include "../app/ComUtils.h"

namespace LiteBrowser {

class TabManager;

class Tab {
public:
    Tab(int id, bool isPrivate, HWND hParentWnd);
    ~Tab();

    int GetId() const { return id_; }
    bool IsPrivate() const { return isPrivate_; }
    bool IsPinned() const { return isPinned_; }
    void SetPinned(bool pinned) { isPinned_ = pinned; }

    bool IsLoading() const { return isLoading_; }
    bool CanGoBack() const { return canGoBack_; }
    bool CanGoForward() const { return canGoForward_; }

    const std::wstring& GetUrl() const { return url_; }
    const std::wstring& GetTitle() const { return title_; }
    const std::wstring& GetOriginalUrl() const { return originalUrl_; }
    bool IsInReaderMode() const { return isInReaderMode_; }
    bool IsReaderAvailable() const { return isReaderAvailable_; }

    int GetBlockedAdsCount() const { return blockedAdsCount_; }
    int GetBlockedTrackersCount() const { return blockedTrackersCount_; }

    // Navigation
    void Navigate(const std::wstring& url);
    void GoBack();
    void GoForward();
    void Reload(bool ignoreCache = false);
    void Stop();

    // Reader Mode
    void ToggleReaderMode();
    void EnterReaderMode();
    void ExitReaderMode();

    // Zoom
    void ZoomIn();
    void ZoomOut();
    void ResetZoom();
    double GetZoom() const { return currentZoom_; }
    void SetZoom(double factor);

    // Find in Page
    void FindInPage(const std::wstring& text, bool forward = true);
    void StopFind();

    // WebView2 Attachment & Resizing
    void AttachWebView(ICoreWebView2Environment* env, std::function<void(bool success)> onCreated);
    void Resize(const RECT& bounds);
    void SetVisible(bool visible);
    void SetFocus();

    // DevTools / Inspect
    void OpenDevTools();

    // Event hooks
    void SetNavigationCallback(std::function<void()> callback) { onStateChanged_ = callback; }

private:
    void SetupEventHandlers();
    void InjectAdBlockCosmetics();
    void CheckReaderAvailability();

    int id_ = 0;
    bool isPrivate_ = false;
    bool isPinned_ = false;
    bool isLoading_ = false;
    bool canGoBack_ = false;
    bool canGoForward_ = false;
    bool isInReaderMode_ = false;
    bool isReaderAvailable_ = false;
    double currentZoom_ = 1.0;

    int blockedAdsCount_ = 0;
    int blockedTrackersCount_ = 0;

    std::wstring url_ = L"lite://newtab";
    std::wstring originalUrl_ = L"";
    std::wstring title_ = L"New Tab";

    HWND hParentWnd_ = nullptr;
    ComPtr<ICoreWebView2Environment> env_;
    ComPtr<ICoreWebView2Controller> controller_;
    ComPtr<ICoreWebView2> webview_;

    EventRegistrationToken navStartingToken_{};
    EventRegistrationToken navCompletedToken_{};
    EventRegistrationToken sourceChangedToken_{};
    EventRegistrationToken titleChangedToken_{};
    EventRegistrationToken webResourceRequestedToken_{};
    EventRegistrationToken processFailedToken_{};
    EventRegistrationToken webMessageToken_{};

    std::function<void()> onStateChanged_;
};

} // namespace LiteBrowser
