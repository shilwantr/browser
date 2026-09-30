#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>

struct sqlite3;

namespace LiteBrowser {

struct BookmarkItem {
    int64_t id = 0;
    std::wstring url;
    std::wstring title;
    std::wstring favicon;
    int64_t folderId = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    int32_t position = 0;
};

struct BookmarkFolder {
    int64_t id = 0;
    std::wstring title;
    int64_t parentId = 0;
    int32_t position = 0;
};

struct HistoryItem {
    int64_t id = 0;
    std::wstring url;
    std::wstring title;
    int64_t visitedAt = 0;
    int32_t visitCount = 1;
};

struct ClosedTabItem {
    int64_t id = 0;
    std::wstring url;
    std::wstring title;
    int64_t closedAt = 0;
    int32_t position = 0;
};

struct SiteSettings {
    std::wstring host;
    int32_t adBlock = 1;      // 1 = Blocked, 0 = Allowed
    int32_t trackerBlock = 1; // 1 = Blocked, 0 = Allowed
    int32_t jsEnabled = 1;    // 1 = Enabled, 0 = Disabled
    int32_t cookiesMode = 1;  // 1 = Allow all, 0 = Block 3rd party
    double zoomLevel = 1.0;   // 1.0 = 100%
};

struct DownloadItem {
    int64_t id = 0;
    std::wstring filePath;
    std::wstring url;
    int64_t totalBytes = 0;
    int64_t receivedBytes = 0;
    std::wstring state; // "in_progress", "completed", "cancelled", "failed"
    int64_t startedAt = 0;
    int64_t completedAt = 0;
};

class StorageManager {
public:
    static StorageManager& GetInstance();

    StorageManager();
    ~StorageManager();

    // Prevent copies
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    bool Initialize(const std::wstring& customDbPath = L"");
    void Close();

    // Bookmarks
    int64_t AddBookmark(const std::wstring& url, const std::wstring& title, int64_t folderId = 0);
    bool DeleteBookmark(int64_t id);
    bool UpdateBookmark(int64_t id, const std::wstring& url, const std::wstring& title, int64_t folderId);
    std::vector<BookmarkItem> GetBookmarks(int64_t folderId = -1);
    std::vector<BookmarkItem> SearchBookmarks(const std::wstring& query);
    bool IsBookmarked(const std::wstring& url);

    // Bookmark Folders
    int64_t AddBookmarkFolder(const std::wstring& title, int64_t parentId = 0);
    std::vector<BookmarkFolder> GetBookmarkFolders();
    bool DeleteBookmarkFolder(int64_t id);

    // History
    bool AddHistory(const std::wstring& url, const std::wstring& title);
    std::vector<HistoryItem> GetHistory(int limit = 100);
    std::vector<HistoryItem> SearchHistory(const std::wstring& query, int limit = 50);
    bool DeleteHistoryEntry(int64_t id);
    bool DeleteHistorySince(int64_t timestamp);
    bool ClearAllHistory();

    // Closed Tabs Stack (up to 20 tabs)
    bool PushClosedTab(const std::wstring& url, const std::wstring& title, int32_t position);
    bool PopClosedTab(ClosedTabItem& outTab);
    std::vector<ClosedTabItem> GetClosedTabs(int limit = 20);

    // Per-Site Settings
    SiteSettings GetSiteSettings(const std::wstring& host);
    bool SetSiteSettings(const SiteSettings& settings);
    double GetSiteZoom(const std::wstring& host);
    bool SetSiteZoom(const std::wstring& host, double zoom);

    // Downloads
    int64_t AddDownload(const std::wstring& filePath, const std::wstring& url, int64_t totalBytes);
    bool UpdateDownloadProgress(int64_t id, int64_t receivedBytes, const std::wstring& state);
    std::vector<DownloadItem> GetDownloads(int limit = 50);

    // Preferences
    std::wstring GetPreference(const std::wstring& key, const std::wstring& defaultValue = L"");
    bool SetPreference(const std::wstring& key, const std::wstring& value);

    // Database Path
    std::wstring GetDatabasePath() const { return dbPath_; }

private:
    bool CreateTables();
    std::wstring GetDefaultDatabasePath();

    sqlite3* db_ = nullptr;
    std::wstring dbPath_;
    std::mutex dbMutex_;
};

} // namespace LiteBrowser
