#include "InternalPages.h"
#include "../app/AppConfig.h"
#include <sstream>

namespace LiteBrowser {

bool InternalPages::IsInternalUrl(const std::wstring& url) {
    return url.rfind(L"lite://", 0) == 0;
}

std::wstring InternalPages::GetPageHtml(const std::wstring& url) {
    if (url == URL_NEWTAB || url == L"lite://newtab/") return GetNewTabHtml();
    if (url == URL_SETTINGS || url == L"lite://settings/") return GetSettingsHtml();
    if (url == URL_HISTORY || url == L"lite://history/") return GetHistoryHtml();
    if (url == URL_BOOKMARKS || url == L"lite://bookmarks/") return GetBookmarksHtml();
    if (url == URL_DOWNLOADS || url == L"lite://downloads/") return GetDownloadsHtml();
    if (url == URL_PRIVACY || url == L"lite://privacy/") return GetPrivacyHtml();
    if (url == URL_ABOUT || url == L"lite://about/") return GetAboutHtml();
    return GetNewTabHtml();
}

std::wstring InternalPages::GetNewTabHtml() {
    auto bookmarks = StorageManager::GetInstance().GetBookmarks();
    auto recent = StorageManager::GetInstance().GetHistory(8);

    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>New Tab</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: #18181a; color: #f0f2f5;
  font-family: 'Segoe UI Variable Text', -apple-system, sans-serif;
  display: flex; flex-direction: column; align-items: center; justify-content: center;
  min-height: 100vh; padding: 20px;
}
.brand { font-size: 32px; font-weight: 300; letter-spacing: 6px; margin-bottom: 36px; color: #ffffff; }
.search-box {
  width: 100%; max-width: 580px; position: relative; margin-bottom: 48px;
}
.search-input {
  width: 100%; padding: 14px 20px; font-size: 15px; border-radius: 8px;
  background: #242426; border: 1px solid #36393f; color: #ffffff;
  outline: none; transition: border-color 0.15s ease;
}
.search-input:focus { border-color: #666; }
.section-title {
  width: 100%; max-width: 580px; font-size: 12px; text-transform: uppercase;
  letter-spacing: 1px; color: #888; margin-bottom: 12px;
}
.grid {
  display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px;
  width: 100%; max-width: 580px; margin-bottom: 36px;
}
.tile {
  background: #202124; border: 1px solid #303236; border-radius: 6px;
  padding: 14px; text-align: center; text-decoration: none; color: #e0e0e0;
  cursor: pointer; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  font-size: 13px; transition: background 0.15s ease;
}
.tile:hover { background: #2c2d30; }
</style>
</head>
<body>
<div class="brand">LITE</div>
<div class="search-box">
  <input class="search-input" id="search" placeholder="Search or enter address" autofocus />
</div>
)raw";

    if (!bookmarks.empty()) {
        ss << L"<div class=\"section-title\">Bookmarks</div>\n<div class=\"grid\">\n";
        size_t count = std::min<size_t>(bookmarks.size(), 8);
        for (size_t i = 0; i < count; ++i) {
            ss << L"<div class=\"tile\" onclick=\"window.chrome.webview.postMessage('open:' + '" << bookmarks[i].url << L"')\">"
               << bookmarks[i].title << L"</div>\n";
        }
        ss << L"</div>\n";
    }

    if (!recent.empty()) {
        ss << L"<div class=\"section-title\">Recent</div>\n<div class=\"grid\">\n";
        for (const auto& item : recent) {
            ss << L"<div class=\"tile\" onclick=\"window.chrome.webview.postMessage('open:' + '" << item.url << L"')\">"
               << item.title << L"</div>\n";
        }
        ss << L"</div>\n";
    }

    ss << LR"raw(
<script>
document.getElementById('search').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    const val = e.target.value.trim();
    if (val) window.chrome.webview.postMessage('navigate:' + val);
  }
});
</script>
</body>
</html>)raw";

    return ss.str();
}

std::wstring InternalPages::GetHistoryHtml() {
    auto history = StorageManager::GetInstance().GetHistory(150);

    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>History - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; padding: 40px; }
.container { max-width: 800px; margin: 0 auto; }
.header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; }
h1 { font-size: 24px; font-weight: 500; }
.btn { background: #28282a; border: 1px solid #444; color: #fff; padding: 6px 14px; border-radius: 4px; cursor: pointer; font-size: 13px; }
.btn:hover { background: #333; }
.item { display: flex; align-items: center; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #2a2a2c; }
.item-left { display: flex; flex-direction: column; overflow: hidden; margin-right: 16px; }
.item-title { font-size: 14px; color: #fff; text-decoration: none; cursor: pointer; }
.item-url { font-size: 12px; color: #888; text-decoration: none; margin-top: 2px; }
.del-btn { color: #888; background: transparent; border: none; cursor: pointer; font-size: 12px; }
.del-btn:hover { color: #f44; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>History</h1>
    <button class="btn" onclick="window.chrome.webview.postMessage('history:clear_all')">Clear All History</button>
  </div>
)raw";

    for (const auto& item : history) {
        ss << L"<div class=\"item\">\n"
           << L"  <div class=\"item-left\">\n"
           << L"    <span class=\"item-title\" onclick=\"window.chrome.webview.postMessage('open:' + '" << item.url << L"')\">" << item.title << L"</span>\n"
           << L"    <span class=\"item-url\">" << item.url << L"</span>\n"
           << L"  </div>\n"
           << L"  <button class=\"del-btn\" onclick=\"window.chrome.webview.postMessage('history:delete:' + '" << item.id << L"')\">Delete</button>\n"
           << L"</div>\n";
    }

    ss << L"</div>\n</body>\n</html>";
    return ss.str();
}

std::wstring InternalPages::GetBookmarksHtml() {
    auto bookmarks = StorageManager::GetInstance().GetBookmarks();

    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Bookmarks - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; padding: 40px; }
.container { max-width: 800px; margin: 0 auto; }
.header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; }
h1 { font-size: 24px; font-weight: 500; }
.actions { display: flex; gap: 10px; }
.btn { background: #28282a; border: 1px solid #444; color: #fff; padding: 6px 14px; border-radius: 4px; cursor: pointer; font-size: 13px; }
.btn:hover { background: #333; }
.item { display: flex; align-items: center; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #2a2a2c; }
.item-title { font-size: 14px; color: #fff; text-decoration: none; cursor: pointer; }
.item-url { font-size: 12px; color: #888; }
.del-btn { color: #888; background: transparent; border: none; cursor: pointer; font-size: 12px; }
.del-btn:hover { color: #f44; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>Bookmarks</h1>
    <div class="actions">
      <button class="btn" onclick="window.chrome.webview.postMessage('import:chrome')">Import Chrome</button>
      <button class="btn" onclick="window.chrome.webview.postMessage('import:edge')">Import Edge</button>
    </div>
  </div>
)raw";

    for (const auto& b : bookmarks) {
        ss << L"<div class=\"item\">\n"
           << L"  <div>\n"
           << L"    <span class=\"item-title\" onclick=\"window.chrome.webview.postMessage('open:' + '" << b.url << L"')\">" << b.title << L"</span>\n"
           << L"    <div class=\"item-url\">" << b.url << L"</div>\n"
           << L"  </div>\n"
           << L"  <button class=\"del-btn\" onclick=\"window.chrome.webview.postMessage('bookmark:delete:' + '" << b.id << L"')\">Delete</button>\n"
           << L"</div>\n";
    }

    ss << L"</div>\n</body>\n</html>";
    return ss.str();
}

std::wstring InternalPages::GetDownloadsHtml() {
    auto downloads = StorageManager::GetInstance().GetDownloads();

    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Downloads - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; padding: 40px; }
.container { max-width: 800px; margin: 0 auto; }
h1 { font-size: 24px; font-weight: 500; margin-bottom: 24px; }
.item { display: flex; align-items: center; justify-content: space-between; padding: 14px 0; border-bottom: 1px solid #2a2a2c; }
.file-name { font-size: 14px; font-weight: 500; color: #fff; }
.file-meta { font-size: 12px; color: #888; margin-top: 4px; }
.btn { background: #28282a; border: 1px solid #444; color: #fff; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-size: 12px; }
.btn:hover { background: #333; }
</style>
</head>
<body>
<div class="container">
  <h1>Downloads</h1>
)raw";

    if (downloads.empty()) {
        ss << L"<p style=\"color: #888;\">No downloads yet.</p>\n";
    } else {
        for (const auto& d : downloads) {
            ss << L"<div class=\"item\">\n"
               << L"  <div>\n"
               << L"    <div class=\"file-name\">" << d.filePath << L"</div>\n"
               << L"    <div class=\"file-meta\">" << d.state << L" &bull; " << (d.receivedBytes / 1024) << L" KB</div>\n"
               << L"  </div>\n"
               << L"  <button class=\"btn\" onclick=\"window.chrome.webview.postMessage('download:open:' + '" << d.filePath << L"')\">Open File</button>\n"
               << L"</div>\n";
        }
    }

    ss << L"</div>\n</body>\n</html>";
    return ss.str();
}

std::wstring InternalPages::GetPrivacyHtml() {
    auto logs = AdBlockEngine::GetInstance().GetRecentRequests(100);
    size_t totalRules = AdBlockEngine::GetInstance().GetTotalRulesCount();
    size_t blockedDomains = AdBlockEngine::GetInstance().GetBlockedDomainsCount();

    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Privacy Inspector - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; padding: 40px; }
.container { max-width: 900px; margin: 0 auto; }
h1 { font-size: 24px; font-weight: 500; margin-bottom: 8px; }
.subtitle { color: #888; font-size: 13px; margin-bottom: 24px; }
.stats { display: flex; gap: 20px; margin-bottom: 30px; }
.card { background: #222225; padding: 18px 24px; border-radius: 6px; border: 1px solid #333; flex: 1; }
.card-val { font-size: 28px; font-weight: 600; color: #fff; }
.card-lbl { font-size: 12px; color: #888; margin-top: 4px; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th, td { padding: 10px 12px; text-align: left; border-bottom: 1px solid #28282b; }
th { color: #888; font-weight: 500; }
.tag-blocked { color: #ff5555; font-weight: 600; }
.tag-allowed { color: #55ff77; }
</style>
</head>
<body>
<div class="container">
  <h1>Privacy & AdBlock Inspector</h1>
  <div class="subtitle">Live local request monitor. Zero telemetry. All filtering runs strictly on your machine.</div>
  <div class="stats">
    <div class="card"><div class="card-val">)raw"
       << totalRules << LR"raw(</div><div class="card-lbl">Active Rules</div></div>
    <div class="card"><div class="card-val">)raw"
       << blockedDomains << LR"raw(</div><div class="card-lbl">Blocked Ad & Tracker Domains</div></div>
  </div>
  <table>
    <thead><tr><th>Status</th><th>Type</th><th>Host</th><th>Rule / URL</th></tr></thead>
    <tbody>
)raw";

    for (const auto& log : logs) {
        std::string ruleStr = log.matchedRule.empty() ? log.url.substr(0, 70) : log.matchedRule;
        ss << L"<tr>"
           << L"<td><span class=\"" << (log.blocked ? L"tag-blocked" : L"tag-allowed") << L"\">"
           << (log.blocked ? L"BLOCKED" : L"ALLOWED") << L"</span></td>"
           << L"<td>" << (log.isTracker ? L"Tracker" : L"Ad/Resource") << L"</td>"
           << L"<td>" << std::wstring(log.documentHost.begin(), log.documentHost.end()) << L"</td>"
           << L"<td>" << std::wstring(ruleStr.begin(), ruleStr.end()) << L"</td>"
           << L"</tr>\n";
    }

    ss << L"</tbody></table></div></body></html>";
    return ss.str();
}

std::wstring InternalPages::GetSettingsHtml() {
    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Settings - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; padding: 40px; }
.container { max-width: 720px; margin: 0 auto; }
h1 { font-size: 24px; font-weight: 500; margin-bottom: 30px; }
.section { margin-bottom: 32px; border-bottom: 1px solid #28282b; padding-bottom: 24px; }
.section-title { font-size: 16px; font-weight: 600; margin-bottom: 16px; }
.row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; font-size: 14px; }
.desc { color: #888; font-size: 12px; margin-top: 2px; }
select, input[type="text"] {
  background: #242426; border: 1px solid #36393f; color: #fff; padding: 6px 12px; border-radius: 4px; outline: none; font-size: 13px;
}
</style>
</head>
<body>
<div class="container">
  <h1>Settings</h1>
  <div class="section">
    <div class="section-title">Search</div>
    <div class="row">
      <div><div>Default Search Engine</div><div class="desc">Used when entering queries in the address bar</div></div>
      <select id="searchEngine" onchange="window.chrome.webview.postMessage('settings:search:' + this.value)">
        <option value="Google">Google</option>
        <option value="DuckDuckGo">DuckDuckGo</option>
        <option value="Bing">Bing</option>
        <option value="Brave Search">Brave Search</option>
      </select>
    </div>
  </div>
  <div class="section">
    <div class="section-title">Privacy & Blocking</div>
    <div class="row">
      <div><div>Block Advertisements</div><div class="desc">Interception at network layer before requests leave browser</div></div>
      <input type="checkbox" id="blockAds" checked onchange="window.chrome.webview.postMessage('settings:adblock:' + this.checked)" />
    </div>
    <div class="row">
      <div><div>Block Trackers & Telemetry</div><div class="desc">Prevents third-party trackers, pixels, and scripts</div></div>
      <input type="checkbox" id="blockTrackers" checked onchange="window.chrome.webview.postMessage('settings:trackers:' + this.checked)" />
    </div>
  </div>
  <div class="section">
    <div class="section-title">Reader Mode</div>
    <div class="row">
      <div><div>Reader Theme</div><div class="desc">Default color scheme for extracted articles</div></div>
      <select onchange="window.chrome.webview.postMessage('settings:reader_theme:' + this.value)">
        <option value="Warm">Warm (Sepia)</option>
        <option value="Light">Light</option>
        <option value="Dark">Dark</option>
      </select>
    </div>
  </div>
</div>
</body>
</html>)raw";
    return ss.str();
}

std::wstring InternalPages::GetAboutHtml() {
    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>About - LITE</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }
.card { background: #202124; border: 1px solid #303236; border-radius: 8px; max-width: 480px; width: 100%; padding: 36px; text-align: center; }
.title { font-size: 26px; font-weight: 300; letter-spacing: 4px; margin-bottom: 8px; color: #fff; }
.ver { color: #888; font-size: 13px; margin-bottom: 24px; }
.desc { color: #bbb; font-size: 14px; line-height: 1.6; margin-bottom: 28px; }
.specs { text-align: left; font-size: 12px; color: #777; border-top: 1px solid #2d2f33; padding-top: 18px; margin-top: 24px; }
.specs div { margin-bottom: 6px; }
</style>
</head>
<body>
<div class="card">
  <div class="title">LITE Browser</div>
  <div class="ver">Version 0.1.0</div>
  <div class="desc">A tiny browser for people who just want the web without the garbage. Small footprint, native Win32 speed, zero telemetry, integrated ad/tracker blocking, and distraction-free Reader Mode.</div>
  <div class="specs">
    <div><strong>Platform:</strong> Windows 10/11 x64</div>
    <div><strong>Runtime:</strong> Microsoft WebView2 Evergreen</div>
    <div><strong>Storage:</strong> SQLite 3 (Local)</div>
    <div><strong>Telemetry:</strong> None (0 bytes sent)</div>
  </div>
</div>
</body>
</html>)raw";
    return ss.str();
}

std::wstring InternalPages::GetErrorHtml(const std::wstring& failedUrl, int errorCode, const std::wstring& errorText) {
    std::wstringstream ss;
    ss << LR"raw(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Can't reach this page</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #18181a; color: #f0f2f5; font-family: 'Segoe UI Variable Text', -apple-system, sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 24px; }
.box { max-width: 520px; width: 100%; }
h1 { font-size: 22px; font-weight: 500; margin-bottom: 12px; }
.url { color: #888; font-size: 13px; margin-bottom: 20px; word-break: break-all; }
.msg { color: #bbb; font-size: 14px; line-height: 1.5; margin-bottom: 28px; }
.btn { background: #2c2d30; border: 1px solid #444; color: #fff; padding: 8px 18px; border-radius: 4px; cursor: pointer; font-size: 13px; }
.btn:hover { background: #38393d; }
.err-code { margin-top: 36px; font-size: 11px; color: #666; font-family: Consolas, monospace; }
</style>
</head>
<body>
<div class="box">
  <h1>Can't reach this page</h1>
  <div class="url">)raw" << failedUrl << LR"raw(</div>
  <div class="msg">Check your internet connection or verify the address was entered correctly.</div>
  <button class="btn" onclick="window.chrome.webview.postMessage('retry')">Reload</button>
  <div class="err-code">Error Code: )raw" << errorCode << L" (" << errorText << L")</div>\n</div>\n</body>\n</html>";
    return ss.str();
}

} // namespace LiteBrowser
