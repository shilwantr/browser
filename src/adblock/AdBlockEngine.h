#pragma once

#include "RuleParser.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <deque>

namespace LiteBrowser {

struct RequestLogEntry {
    std::string url;
    std::string documentHost;
    std::string matchedRule;
    bool blocked = false;
    bool isTracker = false;
    int64_t timestamp = 0;
};

class AdBlockEngine {
public:
    static AdBlockEngine& GetInstance();

    AdBlockEngine();
    ~AdBlockEngine();

    void Initialize();
    void LoadFilterRules(const std::string& rulesContent, bool isTrackerList = false);
    bool LoadFilterFile(const std::wstring& filePath, bool isTrackerList = false);

    // Core matching function called for every web resource request
    bool ShouldBlock(const std::string& requestUrl,
                     const std::string& documentUrl,
                     uint32_t resourceType,
                     bool& outIsTracker,
                     std::string& outMatchedRule);

    // Site Allowlist / Per-site controls
    void SetSiteAdBlock(const std::string& host, bool enabled);
    void SetSiteTrackerBlock(const std::string& host, bool enabled);
    bool IsSiteAdBlockEnabled(const std::string& host);
    bool IsSiteTrackerBlockEnabled(const std::string& host);

    // Privacy debug log for lite://privacy
    std::vector<RequestLogEntry> GetRecentRequests(size_t limit = 100);
    void ClearLog();

    // Statistics
    size_t GetTotalRulesCount() const;
    size_t GetBlockedDomainsCount() const;

private:
    void PreloadBuiltInRules();
    std::string ExtractHost(const std::string& url);
    bool IsThirdParty(const std::string& hostA, const std::string& hostB);

    mutable std::mutex engineMutex_;

    // Fast domain hash tables
    std::unordered_set<std::string> blockedAdDomains_;
    std::unordered_set<std::string> blockedTrackerDomains_;
    std::unordered_set<std::string> exceptionDomains_;

    // Complex pattern rules
    std::vector<FilterRule> adRules_;
    std::vector<FilterRule> trackerRules_;
    std::vector<FilterRule> exceptionRules_;

    // Host override settings: host -> {adBlockEnabled, trackerBlockEnabled}
    std::unordered_map<std::string, bool> siteAdBlockOverrides_;
    std::unordered_map<std::string, bool> siteTrackerBlockOverrides_;

    // Debug log
    std::deque<RequestLogEntry> recentRequests_;
    static constexpr size_t MAX_LOG_ENTRIES = 300;
};

} // namespace LiteBrowser
