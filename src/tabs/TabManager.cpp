#include "TabManager.h"
#include "../storage/StorageManager.h"
#include <shlobj.h>
#include <algorithm>

namespace LiteBrowser {

namespace {

std::wstring GetUserDataFolder(bool isPrivate) {
    wchar_t localApp[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
        std::wstring base = std::wstring(localApp) + L"\\LiteBrowser";
        CreateDirectoryW(base.c_str(), nullptr);
        if (isPrivate) {
            std::wstring priv = base + L"\\PrivateData";
            CreateDirectoryW(priv.c_str(), nullptr);
            return priv;
        } else {
            std::wstring norm = base + L"\\UserData";
            CreateDirectoryW(norm.c_str(), nullptr);
            return norm;
        }
    }
    return isPrivate ? L"PrivateData" : L"UserData";
}

} // namespace

TabManager::TabManager(HWND hParentWnd) : hParentWnd_(hParentWnd) {
}

TabManager::~TabManager() {
    tabs_.clear();
    sharedEnv_.Reset();
    privateEnv_.Reset();
}

void TabManager::InitializeEnvironments(std::function<void(bool success)> onInitComplete) {
    std::wstring standardFolder = GetUserDataFolder(false);

    auto handler = MakeCallback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this, onInitComplete](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (SUCCEEDED(hr) && env) {
                sharedEnv_ = env;
                if (onInitComplete) onInitComplete(true);
            } else {
                if (onInitComplete) onInitComplete(false);
            }
            return S_OK;
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, standardFolder.c_str(), nullptr, handler.Get());
    if (FAILED(hr)) {
        if (onInitComplete) onInitComplete(false);
    }
}

std::shared_ptr<Tab> TabManager::CreateTab(const std::wstring& initialUrl, bool isPrivate) {
    if (!sharedEnv_) return nullptr;

    int newId = nextTabId_++;
    auto tab = std::make_shared<Tab>(newId, isPrivate, hParentWnd_);
    tab->SetNavigationCallback([this]() {
        if (onTabsChanged_) onTabsChanged_();
    });

    tabs_.push_back(tab);
    int newIndex = (int)tabs_.size() - 1;

    tab->Resize(currentBounds_);
    tab->AttachWebView(sharedEnv_.Get(), [this, tab, initialUrl, newIndex](bool success) {
        if (success) {
            tab->Resize(currentBounds_);
            tab->SetVisible(true);
            tab->Navigate(initialUrl);
            SelectTab(newIndex);
        }
    });

    if (onTabsChanged_) onTabsChanged_();
    return tab;
}

bool TabManager::CloseTab(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return false;

    auto tabToClose = tabs_[index];

    // Push to closed tabs stack if non-private
    if (!tabToClose->IsPrivate() && !tabToClose->GetUrl().empty()) {
        StorageManager::GetInstance().PushClosedTab(tabToClose->GetUrl(), tabToClose->GetTitle(), index);
    }

    tabToClose->SetVisible(false);
    tabs_.erase(tabs_.begin() + index);

    if (tabs_.empty()) {
        activeIndex_ = -1;
    } else {
        if (activeIndex_ >= (int)tabs_.size()) {
            activeIndex_ = (int)tabs_.size() - 1;
        }
        SelectTab(activeIndex_);
    }

    if (onTabsChanged_) onTabsChanged_();
    return true;
}

void TabManager::CloseOtherTabs(int keepIndex) {
    if (keepIndex < 0 || keepIndex >= (int)tabs_.size()) return;
    auto keepTab = tabs_[keepIndex];

    for (int i = 0; i < (int)tabs_.size(); ++i) {
        if (i != keepIndex) {
            tabs_[i]->SetVisible(false);
        }
    }

    tabs_.clear();
    tabs_.push_back(keepTab);
    activeIndex_ = 0;
    SelectTab(0);
}

void TabManager::CloseTabsToRight(int fromIndex) {
    if (fromIndex < 0 || fromIndex >= (int)tabs_.size() - 1) return;

    for (size_t i = fromIndex + 1; i < tabs_.size(); ++i) {
        tabs_[i]->SetVisible(false);
    }
    tabs_.erase(tabs_.begin() + fromIndex + 1, tabs_.end());

    if (activeIndex_ >= (int)tabs_.size()) {
        activeIndex_ = (int)tabs_.size() - 1;
        SelectTab(activeIndex_);
    }

    if (onTabsChanged_) onTabsChanged_();
}

void TabManager::DuplicateTab(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return;
    std::wstring url = tabs_[index]->GetUrl();
    CreateTab(url, tabs_[index]->IsPrivate());
}

bool TabManager::ReopenClosedTab() {
    ClosedTabItem item;
    if (StorageManager::GetInstance().PopClosedTab(item)) {
        CreateTab(item.url, false);
        return true;
    }
    return false;
}

void TabManager::SelectTab(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return;

    if (activeIndex_ >= 0 && activeIndex_ < (int)tabs_.size() && activeIndex_ != index) {
        tabs_[activeIndex_]->SetVisible(false);
    }

    activeIndex_ = index;
    auto activeTab = tabs_[activeIndex_];
    activeTab->SetVisible(true);
    activeTab->Resize(currentBounds_);
    activeTab->SetFocus();

    if (onTabsChanged_) onTabsChanged_();
}

void TabManager::NextTab() {
    if (tabs_.size() <= 1) return;
    int next = (activeIndex_ + 1) % (int)tabs_.size();
    SelectTab(next);
}

void TabManager::PreviousTab() {
    if (tabs_.size() <= 1) return;
    int prev = (activeIndex_ - 1 + (int)tabs_.size()) % (int)tabs_.size();
    SelectTab(prev);
}

void TabManager::MoveTab(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= (int)tabs_.size()) return;
    if (toIndex < 0 || toIndex >= (int)tabs_.size() || fromIndex == toIndex) return;

    auto tab = tabs_[fromIndex];
    tabs_.erase(tabs_.begin() + fromIndex);
    tabs_.insert(tabs_.begin() + toIndex, tab);

    if (activeIndex_ == fromIndex) {
        activeIndex_ = toIndex;
    } else if (fromIndex < activeIndex_ && toIndex >= activeIndex_) {
        activeIndex_--;
    } else if (fromIndex > activeIndex_ && toIndex <= activeIndex_) {
        activeIndex_++;
    }

    if (onTabsChanged_) onTabsChanged_();
}

std::shared_ptr<Tab> TabManager::GetActiveTab() const {
    if (activeIndex_ >= 0 && activeIndex_ < (int)tabs_.size()) {
        return tabs_[activeIndex_];
    }
    return nullptr;
}

std::shared_ptr<Tab> TabManager::GetTab(int index) const {
    if (index >= 0 && index < (int)tabs_.size()) {
        return tabs_[index];
    }
    return nullptr;
}

void TabManager::ResizeActiveTab(const RECT& bounds) {
    currentBounds_ = bounds;
    auto active = GetActiveTab();
    if (active) {
        active->Resize(bounds);
    }
}

} // namespace LiteBrowser
