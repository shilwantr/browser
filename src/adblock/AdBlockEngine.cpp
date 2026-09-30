#include "AdBlockEngine.h"
#include <fstream>
#include <algorithm>
#include <chrono>

namespace LiteBrowser {

namespace {

int64_t GetNowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

AdBlockEngine& AdBlockEngine::GetInstance() {
    static AdBlockEngine instance;
    return instance;
}

AdBlockEngine::AdBlockEngine() {
    Initialize();
}

AdBlockEngine::~AdBlockEngine() = default;

void AdBlockEngine::Initialize() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    PreloadBuiltInRules();
}

void AdBlockEngine::PreloadBuiltInRules() {
    // Top Advertising Domains
    const char* adDomains[] = {
        "doubleclick.net", "googleads.g.doubleclick.net", "pagead2.googlesyndication.com",
        "adservice.google.com", "partner.googleadservices.com", "criteo.com", "criteo.net",
        "taboola.com", "outbrain.com", "rubiconproject.com", "adnxs.com", "pubmatic.com",
        "openx.net", "casalemedia.com", "contextweb.com", "yieldmo.com", "smartadserver.com",
        "sovrn.com", "adcolony.com", "admob.com", "unityads.unity3d.com", "inmobi.com",
        "vungle.com", "adroll.com", "bidswitch.net", "triplelift.com", "moatads.com",
        "advertising.com", "revcontent.com", "zergnet.com", "adform.net", "adtechus.com",
        "adblade.com", "adpushup.com", "mgid.com", "popads.net", "propellerads.com",
        "exoclick.com", "buysellads.com", "carbonads.net", "adfox.ru", "adriver.ru",
        "adition.com", "yieldlab.net", "teads.tv", "spotxchange.com", "spotx.tv",
        "tremorhub.com", "smartclip.net", "sharethrough.com", "gumgum.com", "nativo.com"
    };

    for (const char* d : adDomains) {
        blockedAdDomains_.insert(d);
    }

    // Top Trackers, Telemetry & Analytics Domains
    const char* trackerDomains[] = {
        "google-analytics.com", "googletagmanager.com", "analytics.google.com", "ssl.google-analytics.com",
        "hotjar.com", "hotjar.io", "segment.io", "segment.com", "mixpanel.com",
        "quantserve.com", "quantcast.com", "chartbeat.com", "chartbeat.net",
        "crazyegg.com", "fullstory.com", "mouseflow.com", "optimizely.com",
        "branch.io", "adjust.com", "appsflyer.com", "amplitude.com", "clarity.ms",
        "scorecardresearch.com", "stats.wp.com", "pixel.wp.com", "sb.scorecardresearch.com",
        "b.scorecardresearch.com", "pixel.facebook.com", "tr.snapchat.com", "analytics.twitter.com",
        "static.ads-twitter.com", "ads-twitter.com", "ct.pinterest.com", "analytics.tiktok.com",
        "telemetry.microsoft.com", "vortex.data.microsoft.com", "browser.pipe.aria.microsoft.com",
        "in.appcenter.ms", "api.segment.io", "cdn.segment.com", "heapanalytics.com",
        "inspectlet.com", "kissmetrics.com", "luckyorange.com", "matomo.org", "piwik.pro"
    };

    for (const char* d : trackerDomains) {
        blockedTrackerDomains_.insert(d);
    }

    // Common ad keywords and path patterns
    const char* commonAdPatterns[] = {
        "/pagead/", "/adserver/", "/ad_banner/", "/adunit/",
        "/ads/banner_", "/sponsor_ads/", "/doubleclick/", "/ad_iframe/"
    };

    for (const char* p : commonAdPatterns) {
        FilterRule r;
        r.type = FilterRuleType::Block;
        r.pattern = p;
        r.isDomainAnchor = false;
        r.isTracker = false;
        adRules_.push_back(r);
    }
}

std::string AdBlockEngine::ExtractHost(const std::string& url) {
    if (url.empty()) return "";
    size_t start = 0;
    if (url.rfind("https://", 0) == 0) start = 8;
    else if (url.rfind("http://", 0) == 0) start = 7;
    else if (url.rfind("lite://", 0) == 0) return "lite";

    size_t end = url.find_first_of("/:?#", start);
    std::string host = (end == std::string::npos) ? url.substr(start) : url.substr(start, end - start);
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    return host;
}

bool AdBlockEngine::IsThirdParty(const std::string& hostA, const std::string& hostB) {
    if (hostA.empty() || hostB.empty()) return false;
    if (hostA == hostB) return false;

    // Check base domain match (e.g. static.example.com and www.example.com share example.com)
    auto GetBaseDomain = [](const std::string& host) -> std::string {
        size_t lastDot = host.find_last_of('.');
        if (lastDot == std::string::npos || lastDot == 0) return host;
        size_t secondLastDot = host.find_last_of('.', lastDot - 1);
        if (secondLastDot == std::string::npos) return host;
        return host.substr(secondLastDot + 1);
    };

    return GetBaseDomain(hostA) != GetBaseDomain(hostB);
}

void AdBlockEngine::LoadFilterRules(const std::string& rulesContent, bool isTrackerList) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    auto rules = RuleParser::ParseBuffer(rulesContent);
    for (auto& r : rules) {
        r.isTracker = isTrackerList;
        if (r.type == FilterRuleType::Exception) {
            if (r.isDomainAnchor && !r.domainAnchor.empty()) {
                exceptionDomains_.insert(r.domainAnchor);
            }
            exceptionRules_.push_back(std::move(r));
        } else {
            if (r.isDomainAnchor && r.pattern.empty()) {
                if (isTrackerList) {
                    blockedTrackerDomains_.insert(r.domainAnchor);
                } else {
                    blockedAdDomains_.insert(r.domainAnchor);
                }
            } else {
                if (isTrackerList) {
                    trackerRules_.push_back(std::move(r));
                } else {
                    adRules_.push_back(std::move(r));
                }
            }
        }
    }
}

bool AdBlockEngine::LoadFilterFile(const std::wstring& filePath, bool isTrackerList) {
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    LoadFilterRules(content, isTrackerList);
    return true;
}

bool AdBlockEngine::ShouldBlock(const std::string& requestUrl,
                               const std::string& documentUrl,
                               uint32_t resourceType,
                               bool& outIsTracker,
                               std::string& outMatchedRule) {
    std::lock_guard<std::mutex> lock(engineMutex_);

    outIsTracker = false;
    outMatchedRule = "";

    std::string reqHost = ExtractHost(requestUrl);
    std::string docHost = ExtractHost(documentUrl);

    // If internal page or blank, do not block
    if (docHost == "lite" || reqHost.empty() || reqHost == "lite") return false;

    // Check per-site overrides for document host
    bool adBlockAllowedForSite = false;
    auto adIt = siteAdBlockOverrides_.find(docHost);
    if (adIt != siteAdBlockOverrides_.end() && !adIt->second) {
        adBlockAllowedForSite = true; // Ads explicitly allowed on this site
    }

    bool trackerBlockAllowedForSite = false;
    auto trIt = siteTrackerBlockOverrides_.find(docHost);
    if (trIt != siteTrackerBlockOverrides_.end() && !trIt->second) {
        trackerBlockAllowedForSite = true; // Trackers explicitly allowed on this site
    }

    // Check exceptions
    if (exceptionDomains_.find(reqHost) != exceptionDomains_.end()) return false;

    // Helper to check domain hierarchy in set: "sub.example.com" -> checks "sub.example.com", "example.com"
    auto MatchesDomainSet = [](const std::unordered_set<std::string>& domainSet, const std::string& host) -> bool {
        if (domainSet.find(host) != domainSet.end()) return true;
        size_t dotPos = host.find('.');
        while (dotPos != std::string::npos) {
            std::string sub = host.substr(dotPos + 1);
            if (domainSet.find(sub) != domainSet.end()) return true;
            dotPos = host.find('.', dotPos + 1);
        }
        return false;
    };

    bool isBlocked = false;

    // 1. Check Trackers
    if (!trackerBlockAllowedForSite && MatchesDomainSet(blockedTrackerDomains_, reqHost)) {
        isBlocked = true;
        outIsTracker = true;
        outMatchedRule = "||" + reqHost + "^ (tracker)";
    }

    // 2. Check Ads
    if (!isBlocked && !adBlockAllowedForSite && MatchesDomainSet(blockedAdDomains_, reqHost)) {
        isBlocked = true;
        outIsTracker = false;
        outMatchedRule = "||" + reqHost + "^ (ad)";
    }

    // 3. Check pattern rules if not blocked by domain
    if (!isBlocked && !adBlockAllowedForSite) {
        for (const auto& r : adRules_) {
            if (!r.pattern.empty() && requestUrl.find(r.pattern) != std::string::npos) {
                if (r.isThirdPartyOnly && !IsThirdParty(reqHost, docHost)) continue;
                isBlocked = true;
                outIsTracker = false;
                outMatchedRule = r.pattern;
                break;
            }
        }
    }

    if (!isBlocked && !trackerBlockAllowedForSite) {
        for (const auto& r : trackerRules_) {
            if (!r.pattern.empty() && requestUrl.find(r.pattern) != std::string::npos) {
                if (r.isThirdPartyOnly && !IsThirdParty(reqHost, docHost)) continue;
                isBlocked = true;
                outIsTracker = true;
                outMatchedRule = r.pattern;
                break;
            }
        }
    }

    // Record in debug log
    RequestLogEntry entry;
    entry.url = requestUrl;
    entry.documentHost = docHost;
    entry.matchedRule = outMatchedRule;
    entry.blocked = isBlocked;
    entry.isTracker = outIsTracker;
    entry.timestamp = GetNowSec();

    recentRequests_.push_front(std::move(entry));
    if (recentRequests_.size() > MAX_LOG_ENTRIES) {
        recentRequests_.pop_back();
    }

    return isBlocked;
}

void AdBlockEngine::SetSiteAdBlock(const std::string& host, bool enabled) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    siteAdBlockOverrides_[host] = enabled;
}

void AdBlockEngine::SetSiteTrackerBlock(const std::string& host, bool enabled) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    siteTrackerBlockOverrides_[host] = enabled;
}

bool AdBlockEngine::IsSiteAdBlockEnabled(const std::string& host) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    auto it = siteAdBlockOverrides_.find(host);
    return (it == siteAdBlockOverrides_.end()) ? true : it->second;
}

bool AdBlockEngine::IsSiteTrackerBlockEnabled(const std::string& host) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    auto it = siteTrackerBlockOverrides_.find(host);
    return (it == siteTrackerBlockOverrides_.end()) ? true : it->second;
}

std::vector<RequestLogEntry> AdBlockEngine::GetRecentRequests(size_t limit) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    std::vector<RequestLogEntry> result;
    size_t count = std::min(limit, recentRequests_.size());
    for (size_t i = 0; i < count; ++i) {
        result.push_back(recentRequests_[i]);
    }
    return result;
}

void AdBlockEngine::ClearLog() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    recentRequests_.clear();
}

size_t AdBlockEngine::GetTotalRulesCount() const {
    std::lock_guard<std::mutex> lock(engineMutex_);
    return blockedAdDomains_.size() + blockedTrackerDomains_.size() + adRules_.size() + trackerRules_.size();
}

size_t AdBlockEngine::GetBlockedDomainsCount() const {
    std::lock_guard<std::mutex> lock(engineMutex_);
    return blockedAdDomains_.size() + blockedTrackerDomains_.size();
}

} // namespace LiteBrowser
