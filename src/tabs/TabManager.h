#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <windows.h>
#include "Tab.h"

namespace LiteBrowser {

class TabManager {
public:
    TabManager(HWND hParentWnd);
    ~TabManager();

    void InitializeEnvironments(std::function<void(bool success)> onInitComplete);

    std::shared_ptr<Tab> CreateTab(const std::wstring& initialUrl = L"lite://newtab", bool isPrivate = false);
    bool CloseTab(int index);
    void CloseOtherTabs(int keepIndex);
    void CloseTabsToRight(int fromIndex);
    void DuplicateTab(int index);
    bool ReopenClosedTab();

    void SelectTab(int index);
    void NextTab();
    void PreviousTab();
    void MoveTab(int fromIndex, int toIndex);

    std::shared_ptr<Tab> GetActiveTab() const;
    std::shared_ptr<Tab> GetTab(int index) const;
    int GetTabCount() const { return (int)tabs_.size(); }
    int GetActiveIndex() const { return activeIndex_; }

    void ResizeActiveTab(const RECT& bounds);
    void SetTabChangeCallback(std::function<void()> callback) { onTabsChanged_ = callback; }

    ICoreWebView2Environment* GetSharedEnv() const { return sharedEnv_.Get(); }

private:
    HWND hParentWnd_ = nullptr;
    int nextTabId_ = 1;
    int activeIndex_ = -1;
    std::vector<std::shared_ptr<Tab>> tabs_;
    RECT currentBounds_{};

    ComPtr<ICoreWebView2Environment> sharedEnv_;
    ComPtr<ICoreWebView2Environment> privateEnv_;

    std::function<void()> onTabsChanged_;
};

} // namespace LiteBrowser
