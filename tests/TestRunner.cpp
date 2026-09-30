#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <windows.h>
#include "../src/storage/StorageManager.h"
#include "../src/adblock/RuleParser.h"
#include "../src/adblock/AdBlockEngine.h"
#include "../src/reader/ReaderModeEngine.h"
#include "../src/navigation/InternalPages.h"
#include "../src/bookmarks/BookmarkImporter.h"

using namespace LiteBrowser;

int g_passCount = 0;
int g_failCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            std::cout << "  [PASS] " << msg << std::endl; \
            g_passCount++; \
        } else { \
            std::cerr << "  [FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            g_failCount++; \
        } \
    } while(0)

void TestStorage() {
    std::cout << "\n=== Running Storage Tests ===" << std::endl;

    // Use memory or test database
    StorageManager storage;
    bool ok = storage.Initialize(L"test_browser.db");
    TEST_ASSERT(ok, "StorageManager initialization");

    // 1. Bookmarks
    int64_t b1 = storage.AddBookmark(L"https://example.com", L"Example Domain");
    TEST_ASSERT(b1 > 0, "Bookmark insertion");
    TEST_ASSERT(storage.IsBookmarked(L"https://example.com"), "Bookmark presence check");

    auto bList = storage.SearchBookmarks(L"Example");
    TEST_ASSERT(!bList.empty() && bList[0].url == L"https://example.com", "Bookmark search");

    bool delOk = storage.DeleteBookmark(b1);
    TEST_ASSERT(delOk, "Bookmark deletion");
    TEST_ASSERT(!storage.IsBookmarked(L"https://example.com"), "Bookmark deleted confirmation");

    // 2. History
    storage.AddHistory(L"https://wikipedia.org", L"Wikipedia");
    storage.AddHistory(L"https://wikipedia.org", L"Wikipedia - Free Encyclopedia");
    auto hList = storage.GetHistory(10);
    TEST_ASSERT(!hList.empty(), "History insertion");
    TEST_ASSERT(hList[0].visitCount >= 2, "History visit count increment");

    storage.ClearAllHistory();
    auto hEmpty = storage.GetHistory(10);
    TEST_ASSERT(hEmpty.empty(), "History clear all");

    // 3. Closed Tabs
    storage.PushClosedTab(L"https://news.ycombinator.com", L"Hacker News", 1);
    ClosedTabItem popped;
    bool popOk = storage.PopClosedTab(popped);
    TEST_ASSERT(popOk && popped.url == L"https://news.ycombinator.com", "Closed tab push and pop");

    // 4. Site Settings
    SiteSettings s;
    s.host = L"example.org";
    s.adBlock = 0; // Allowed
    s.zoomLevel = 1.25;
    storage.SetSiteSettings(s);

    SiteSettings retrieved = storage.GetSiteSettings(L"example.org");
    TEST_ASSERT(retrieved.adBlock == 0 && retrieved.zoomLevel == 1.25, "Site settings persistence");

    storage.Close();
    DeleteFileW(L"test_browser.db");
}

void TestAdBlocking() {
    std::cout << "\n=== Running Ad & Tracker Blocking Tests ===" << std::endl;

    // 1. RuleParser
    FilterRule r1;
    bool p1 = RuleParser::ParseLine("||doubleclick.net^$third-party", r1);
    TEST_ASSERT(p1, "Rule parser: domain anchor rule");
    TEST_ASSERT(r1.isDomainAnchor && r1.domainAnchor == "doubleclick.net", "Rule parser: domain anchor extraction");
    TEST_ASSERT(r1.isThirdPartyOnly, "Rule parser: third-party option flag");

    FilterRule r2;
    bool p2 = RuleParser::ParseLine("@@||safe.doubleclick.net^", r2);
    TEST_ASSERT(p2 && r2.type == FilterRuleType::Exception, "Rule parser: exception whitelist rule");

    // 2. AdBlockEngine
    AdBlockEngine engine;
    engine.Initialize();

    bool isTracker = false;
    std::string matched;

    // Test Ad Blocking
    bool blockedAd = engine.ShouldBlock("https://googleads.g.doubleclick.net/pagead/ads.js",
                                        "https://example.com/index.html", 0, isTracker, matched);
    TEST_ASSERT(blockedAd && !isTracker, "AdBlockEngine: block DoubleClick ad request");

    // Test Tracker Blocking
    bool blockedTrk = engine.ShouldBlock("https://www.google-analytics.com/analytics.js",
                                         "https://example.com/index.html", 0, isTracker, matched);
    TEST_ASSERT(blockedTrk && isTracker, "AdBlockEngine: block Google Analytics tracker request");

    // Test Clean Request
    bool cleanReq = engine.ShouldBlock("https://cdn.example.com/styles.css",
                                       "https://example.com/index.html", 0, isTracker, matched);
    TEST_ASSERT(!cleanReq, "AdBlockEngine: allow legitimate first-party resource");

    // Test Site Override
    engine.SetSiteAdBlock("example.com", false); // Allow ads on example.com
    bool bypassed = engine.ShouldBlock("https://googleads.g.doubleclick.net/pagead/ads.js",
                                       "https://example.com/index.html", 0, isTracker, matched);
    TEST_ASSERT(!bypassed, "AdBlockEngine: site allowlist override allows ads on specified site");
}

void TestReaderMode() {
    std::cout << "\n=== Running Reader Mode Tests ===" << std::endl;

    ReaderModeEngine& engine = ReaderModeEngine::GetInstance();

    // 1. Article Scoring
    ArticleMetadata meta;
    meta.title = L"Understanding Modern Systems";
    meta.author = L"Jane Doe";
    meta.publishedDate = L"2026-09-30";
    meta.wordCount = 850;
    meta.heroImage = L"https://example.com/hero.jpg";

    int score = engine.CalculateConfidenceScore(meta);
    TEST_ASSERT(score >= 70, "ReaderMode: article confidence score calculation");

    // 2. HTML Sanitization (Crucial Security Test)
    std::wstring dirtyHtml = LR"raw(
        <div>
            <h2>Valid Section Heading</h2>
            <p>This is a safe and informative article paragraph.</p>
            <script>alert('malicious XSS payload');</script>
            <iframe src="http://evil.com/phish"></iframe>
            <a href="javascript:stealCookies()">Click for free money</a>
            <img src="https://example.com/safe.jpg" onerror="evil()" alt="Good photo" />
            <form action="http://evil.com"><input type="password"/></form>
        </div>
    )raw";

    std::wstring cleanHtml = engine.SanitizeHtml(dirtyHtml);

    TEST_ASSERT(cleanHtml.find(L"<script") == std::wstring::npos, "Security: <script> completely stripped");
    TEST_ASSERT(cleanHtml.find(L"<iframe") == std::wstring::npos, "Security: <iframe> completely stripped");
    TEST_ASSERT(cleanHtml.find(L"<form") == std::wstring::npos, "Security: <form> completely stripped");
    TEST_ASSERT(cleanHtml.find(L"javascript:") == std::wstring::npos, "Security: javascript: pseudo-protocol stripped");
    TEST_ASSERT(cleanHtml.find(L"onerror=") == std::wstring::npos, "Security: inline event handler onerror stripped");
    TEST_ASSERT(cleanHtml.find(L"Valid Section Heading") != std::wstring::npos, "Reader content: clean heading preserved");
    TEST_ASSERT(cleanHtml.find(L"safe and informative article paragraph") != std::wstring::npos, "Reader content: text preserved");

    // 3. Reader Document Generation
    meta.sanitizedHtml = cleanHtml;
    ReaderPreferences prefs = engine.GetPreferences();
    std::wstring doc = engine.GenerateReaderHtml(meta, prefs);
    TEST_ASSERT(doc.find(L"Content-Security-Policy") != std::wstring::npos, "Reader Mode: strict CSP included");
    TEST_ASSERT(doc.find(L"Understanding Modern Systems") != std::wstring::npos, "Reader Mode: title rendered");
    TEST_ASSERT(doc.find(L"Jane Doe") != std::wstring::npos, "Reader Mode: author rendered");
}

void TestInternalPages() {
    std::cout << "\n=== Running Internal Pages Tests ===" << std::endl;

    TEST_ASSERT(InternalPages::IsInternalUrl(L"lite://newtab"), "InternalPages: recognizes lite://newtab");
    TEST_ASSERT(InternalPages::IsInternalUrl(L"lite://settings"), "InternalPages: recognizes lite://settings");
    TEST_ASSERT(!InternalPages::IsInternalUrl(L"https://google.com"), "InternalPages: rejects external URLs");

    std::wstring newTabHtml = InternalPages::GetPageHtml(L"lite://newtab");
    TEST_ASSERT(!newTabHtml.empty() && newTabHtml.find(L"LITE") != std::wstring::npos, "InternalPages: generates newtab HTML");

    std::wstring privacyHtml = InternalPages::GetPageHtml(L"lite://privacy");
    TEST_ASSERT(!privacyHtml.empty() && privacyHtml.find(L"Privacy") != std::wstring::npos, "InternalPages: generates privacy HTML");

    std::wstring errHtml = InternalPages::GetErrorHtml(L"https://unreachable.org", -105, L"ERR_NAME_NOT_RESOLVED");
    TEST_ASSERT(!errHtml.empty() && errHtml.find(L"ERR_NAME_NOT_RESOLVED") != std::wstring::npos, "InternalPages: generates clean error page");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " LITE Browser Automated Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    TestStorage();
    TestAdBlocking();
    TestReaderMode();
    TestInternalPages();

    std::cout << "\n========================================" << std::endl;
    std::cout << " Tests Passed: " << g_passCount << std::endl;
    std::cout << " Tests Failed: " << g_failCount << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_failCount == 0) ? 0 : 1;
}
