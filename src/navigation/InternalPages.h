#pragma once

#include <string>
#include <vector>
#include "../storage/StorageManager.h"
#include "../adblock/AdBlockEngine.h"

namespace LiteBrowser {

class InternalPages {
public:
    static bool IsInternalUrl(const std::wstring& url);
    static std::wstring GetPageHtml(const std::wstring& url);

    static std::wstring GetNewTabHtml();
    static std::wstring GetSettingsHtml();
    static std::wstring GetHistoryHtml();
    static std::wstring GetBookmarksHtml();
    static std::wstring GetDownloadsHtml();
    static std::wstring GetPrivacyHtml();
    static std::wstring GetAboutHtml();
    static std::wstring GetErrorHtml(const std::wstring& failedUrl, int errorCode, const std::wstring& errorText);
};

} // namespace LiteBrowser
