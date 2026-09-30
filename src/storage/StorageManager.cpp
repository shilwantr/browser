#include "StorageManager.h"
#include "sqlite3.h"
#include <shlobj.h>
#include <windows.h>
#include <chrono>
#include <sstream>

namespace LiteBrowser {

namespace {

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8) return std::wstring();
    int len = (int)strlen(utf8);
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, &result[0], size);
    return result;
}

int64_t GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

StorageManager& StorageManager::GetInstance() {
    static StorageManager instance;
    return instance;
}

StorageManager::StorageManager() = default;

StorageManager::~StorageManager() {
    Close();
}

std::wstring StorageManager::GetDefaultDatabasePath() {
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        std::wstring dir = std::wstring(localAppData) + L"\\LiteBrowser";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\browser.db";
    }
    return L"browser.db";
}

bool StorageManager::Initialize(const std::wstring& customDbPath) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) return true;

    dbPath_ = customDbPath.empty() ? GetDefaultDatabasePath() : customDbPath;
    std::string pathUtf8 = WideToUtf8(dbPath_);

    int rc = sqlite3_open_v2(pathUtf8.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Enable WAL mode and normal synchronous for speed and reliability
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    return CreateTables();
}

void StorageManager::Close() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

bool StorageManager::CreateTables() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS bookmark_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            parent_id INTEGER DEFAULT 0,
            position INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS bookmarks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT NOT NULL,
            favicon TEXT DEFAULT '',
            folder_id INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            position INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT NOT NULL,
            visited_at INTEGER NOT NULL,
            visit_count INTEGER DEFAULT 1
        );

        CREATE INDEX IF NOT EXISTS idx_history_url ON history(url);
        CREATE INDEX IF NOT EXISTS idx_history_visited ON history(visited_at DESC);

        CREATE TABLE IF NOT EXISTS closed_tabs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT NOT NULL,
            closed_at INTEGER NOT NULL,
            position INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS site_settings (
            host TEXT PRIMARY KEY,
            ad_block INTEGER DEFAULT 1,
            tracker_block INTEGER DEFAULT 1,
            js_enabled INTEGER DEFAULT 1,
            cookies_mode INTEGER DEFAULT 1,
            zoom_level REAL DEFAULT 1.0
        );

        CREATE TABLE IF NOT EXISTS downloads (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            url TEXT NOT NULL,
            total_bytes INTEGER DEFAULT 0,
            received_bytes INTEGER DEFAULT 0,
            state TEXT DEFAULT 'in_progress',
            started_at INTEGER NOT NULL,
            completed_at INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS preferences (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

int64_t StorageManager::AddBookmark(const std::wstring& url, const std::wstring& title, int64_t folderId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return 0;

    const char* sql = "INSERT INTO bookmarks (url, title, folder_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    std::string u8Url = WideToUtf8(url);
    std::string u8Title = WideToUtf8(title);
    int64_t now = GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, u8Title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, folderId);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, now);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(db_) : 0;
}

bool StorageManager::DeleteBookmark(int64_t id) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM bookmarks WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool StorageManager::UpdateBookmark(int64_t id, const std::wstring& url, const std::wstring& title, int64_t folderId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "UPDATE bookmarks SET url = ?, title = ?, folder_id = ?, updated_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8Url = WideToUtf8(url);
    std::string u8Title = WideToUtf8(title);
    int64_t now = GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, u8Title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, folderId);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<BookmarkItem> StorageManager::GetBookmarks(int64_t folderId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<BookmarkItem> items;
    if (!db_) return items;

    std::string sql = (folderId < 0) 
        ? "SELECT id, url, title, favicon, folder_id, created_at, updated_at, position FROM bookmarks ORDER BY position ASC, id ASC;"
        : "SELECT id, url, title, favicon, folder_id, created_at, updated_at, position FROM bookmarks WHERE folder_id = ? ORDER BY position ASC, id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return items;

    if (folderId >= 0) {
        sqlite3_bind_int64(stmt, 1, folderId);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BookmarkItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        item.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        item.favicon = Utf8ToWide((const char*)sqlite3_column_text(stmt, 3));
        item.folderId = sqlite3_column_int64(stmt, 4);
        item.createdAt = sqlite3_column_int64(stmt, 5);
        item.updatedAt = sqlite3_column_int64(stmt, 6);
        item.position = sqlite3_column_int(stmt, 7);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

std::vector<BookmarkItem> StorageManager::SearchBookmarks(const std::wstring& query) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<BookmarkItem> items;
    if (!db_) return items;

    const char* sql = "SELECT id, url, title, favicon, folder_id, created_at, updated_at, position "
                      "FROM bookmarks WHERE title LIKE ? OR url LIKE ? ORDER BY title ASC LIMIT 30;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return items;

    std::string pattern = "%" + WideToUtf8(query) + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BookmarkItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        item.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        item.favicon = Utf8ToWide((const char*)sqlite3_column_text(stmt, 3));
        item.folderId = sqlite3_column_int64(stmt, 4);
        item.createdAt = sqlite3_column_int64(stmt, 5);
        item.updatedAt = sqlite3_column_int64(stmt, 6);
        item.position = sqlite3_column_int(stmt, 7);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

bool StorageManager::IsBookmarked(const std::wstring& url) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "SELECT 1 FROM bookmarks WHERE url = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8Url = WideToUtf8(url);
    sqlite3_bind_text(stmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

int64_t StorageManager::AddBookmarkFolder(const std::wstring& title, int64_t parentId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return 0;

    const char* sql = "INSERT INTO bookmark_folders (title, parent_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    std::string u8Title = WideToUtf8(title);
    sqlite3_bind_text(stmt, 1, u8Title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, parentId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(db_) : 0;
}

std::vector<BookmarkFolder> StorageManager::GetBookmarkFolders() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<BookmarkFolder> folders;
    if (!db_) return folders;

    const char* sql = "SELECT id, title, parent_id, position FROM bookmark_folders ORDER BY position ASC, id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return folders;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BookmarkFolder f;
        f.id = sqlite3_column_int64(stmt, 0);
        f.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        f.parentId = sqlite3_column_int64(stmt, 2);
        f.position = sqlite3_column_int(stmt, 3);
        folders.push_back(f);
    }
    sqlite3_finalize(stmt);
    return folders;
}

bool StorageManager::DeleteBookmarkFolder(int64_t id) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    // Delete folder and its contained bookmarks
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* sql1 = "DELETE FROM bookmarks WHERE folder_id = ?;";
    sqlite3_stmt* stmt1 = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt1, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt1, 1, id);
        sqlite3_step(stmt1);
        sqlite3_finalize(stmt1);
    }

    const char* sql2 = "DELETE FROM bookmark_folders WHERE id = ?;";
    sqlite3_stmt* stmt2 = nullptr;
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt2, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt2, 1, id);
        sqlite3_step(stmt2);
        sqlite3_finalize(stmt2);
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool StorageManager::AddHistory(const std::wstring& url, const std::wstring& title) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_ || url.empty()) return false;

    // Don't store internal scheme history
    if (url.rfind(L"lite://", 0) == 0) return false;

    std::string u8Url = WideToUtf8(url);
    std::string u8Title = WideToUtf8(title);
    int64_t now = GetCurrentTimestamp();

    // Check if URL exists within recent history
    const char* checkSql = "SELECT id, visit_count FROM history WHERE url = ? LIMIT 1;";
    sqlite3_stmt* checkStmt = nullptr;
    int64_t existingId = 0;
    int visitCount = 1;

    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            existingId = sqlite3_column_int64(checkStmt, 0);
            visitCount = sqlite3_column_int(checkStmt, 1) + 1;
        }
        sqlite3_finalize(checkStmt);
    }

    if (existingId > 0) {
        const char* updSql = "UPDATE history SET title = ?, visited_at = ?, visit_count = ? WHERE id = ?;";
        sqlite3_stmt* updStmt = nullptr;
        if (sqlite3_prepare_v2(db_, updSql, -1, &updStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(updStmt, 1, u8Title.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(updStmt, 2, now);
            sqlite3_bind_int(updStmt, 3, visitCount);
            sqlite3_bind_int64(updStmt, 4, existingId);
            sqlite3_step(updStmt);
            sqlite3_finalize(updStmt);
            return true;
        }
    } else {
        const char* insSql = "INSERT INTO history (url, title, visited_at, visit_count) VALUES (?, ?, ?, 1);";
        sqlite3_stmt* insStmt = nullptr;
        if (sqlite3_prepare_v2(db_, insSql, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(insStmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 2, u8Title.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(insStmt, 3, now);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
            return true;
        }
    }
    return false;
}

std::vector<HistoryItem> StorageManager::GetHistory(int limit) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<HistoryItem> items;
    if (!db_) return items;

    const char* sql = "SELECT id, url, title, visited_at, visit_count FROM history ORDER BY visited_at DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return items;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        item.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        item.visitedAt = sqlite3_column_int64(stmt, 3);
        item.visitCount = sqlite3_column_int(stmt, 4);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

std::vector<HistoryItem> StorageManager::SearchHistory(const std::wstring& query, int limit) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<HistoryItem> items;
    if (!db_) return items;

    const char* sql = "SELECT id, url, title, visited_at, visit_count FROM history "
                      "WHERE title LIKE ? OR url LIKE ? ORDER BY visited_at DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return items;

    std::string pattern = "%" + WideToUtf8(query) + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        item.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        item.visitedAt = sqlite3_column_int64(stmt, 3);
        item.visitCount = sqlite3_column_int(stmt, 4);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

bool StorageManager::DeleteHistoryEntry(int64_t id) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM history WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool StorageManager::DeleteHistorySince(int64_t timestamp) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM history WHERE visited_at >= ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, timestamp);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool StorageManager::ClearAllHistory() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    return sqlite3_exec(db_, "DELETE FROM history;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool StorageManager::PushClosedTab(const std::wstring& url, const std::wstring& title, int32_t position) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_ || url.empty()) return false;

    const char* sql = "INSERT INTO closed_tabs (url, title, closed_at, position) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8Url = WideToUtf8(url);
    std::string u8Title = WideToUtf8(title);
    int64_t now = GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, u8Url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, u8Title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_int(stmt, 4, position);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Keep closed_tabs bounded to latest 20
    sqlite3_exec(db_, "DELETE FROM closed_tabs WHERE id NOT IN (SELECT id FROM closed_tabs ORDER BY id DESC LIMIT 20);", nullptr, nullptr, nullptr);
    return true;
}

bool StorageManager::PopClosedTab(ClosedTabItem& outTab) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "SELECT id, url, title, closed_at, position FROM closed_tabs ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outTab.id = sqlite3_column_int64(stmt, 0);
        outTab.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        outTab.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        outTab.closedAt = sqlite3_column_int64(stmt, 3);
        outTab.position = sqlite3_column_int(stmt, 4);
        found = true;
    }
    sqlite3_finalize(stmt);

    if (found) {
        const char* delSql = "DELETE FROM closed_tabs WHERE id = ?;";
        sqlite3_stmt* delStmt = nullptr;
        if (sqlite3_prepare_v2(db_, delSql, -1, &delStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(delStmt, 1, outTab.id);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
        }
    }
    return found;
}

std::vector<ClosedTabItem> StorageManager::GetClosedTabs(int limit) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<ClosedTabItem> tabs;
    if (!db_) return tabs;

    const char* sql = "SELECT id, url, title, closed_at, position FROM closed_tabs ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return tabs;

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClosedTabItem tab;
        tab.id = sqlite3_column_int64(stmt, 0);
        tab.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        tab.title = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        tab.closedAt = sqlite3_column_int64(stmt, 3);
        tab.position = sqlite3_column_int(stmt, 4);
        tabs.push_back(tab);
    }
    sqlite3_finalize(stmt);
    return tabs;
}

SiteSettings StorageManager::GetSiteSettings(const std::wstring& host) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    SiteSettings settings;
    settings.host = host;
    if (!db_ || host.empty()) return settings;

    const char* sql = "SELECT ad_block, tracker_block, js_enabled, cookies_mode, zoom_level FROM site_settings WHERE host = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return settings;

    std::string u8Host = WideToUtf8(host);
    sqlite3_bind_text(stmt, 1, u8Host.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.adBlock = sqlite3_column_int(stmt, 0);
        settings.trackerBlock = sqlite3_column_int(stmt, 1);
        settings.jsEnabled = sqlite3_column_int(stmt, 2);
        settings.cookiesMode = sqlite3_column_int(stmt, 3);
        settings.zoomLevel = sqlite3_column_double(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return settings;
}

bool StorageManager::SetSiteSettings(const SiteSettings& settings) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_ || settings.host.empty()) return false;

    const char* sql = "INSERT INTO site_settings (host, ad_block, tracker_block, js_enabled, cookies_mode, zoom_level) "
                      "VALUES (?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(host) DO UPDATE SET ad_block = excluded.ad_block, tracker_block = excluded.tracker_block, "
                      "js_enabled = excluded.js_enabled, cookies_mode = excluded.cookies_mode, zoom_level = excluded.zoom_level;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8Host = WideToUtf8(settings.host);
    sqlite3_bind_text(stmt, 1, u8Host.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, settings.adBlock);
    sqlite3_bind_int(stmt, 3, settings.trackerBlock);
    sqlite3_bind_int(stmt, 4, settings.jsEnabled);
    sqlite3_bind_int(stmt, 5, settings.cookiesMode);
    sqlite3_bind_double(stmt, 6, settings.zoomLevel);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

double StorageManager::GetSiteZoom(const std::wstring& host) {
    return GetSiteSettings(host).zoomLevel;
}

bool StorageManager::SetSiteZoom(const std::wstring& host, double zoom) {
    SiteSettings s = GetSiteSettings(host);
    s.zoomLevel = zoom;
    return SetSiteSettings(s);
}

int64_t StorageManager::AddDownload(const std::wstring& filePath, const std::wstring& url, int64_t totalBytes) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return 0;

    const char* sql = "INSERT INTO downloads (file_path, url, total_bytes, received_bytes, state, started_at) "
                      "VALUES (?, ?, ?, 0, 'in_progress', ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    std::string u8Path = WideToUtf8(filePath);
    std::string u8Url = WideToUtf8(url);
    int64_t now = GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, u8Path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, u8Url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, totalBytes);
    sqlite3_bind_int64(stmt, 4, now);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(db_) : 0;
}

bool StorageManager::UpdateDownloadProgress(int64_t id, int64_t receivedBytes, const std::wstring& state) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "UPDATE downloads SET received_bytes = ?, state = ?, "
                      "completed_at = CASE WHEN ? IN ('completed', 'cancelled', 'failed') THEN ? ELSE completed_at END "
                      "WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8State = WideToUtf8(state);
    int64_t now = GetCurrentTimestamp();

    sqlite3_bind_int64(stmt, 1, receivedBytes);
    sqlite3_bind_text(stmt, 2, u8State.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, u8State.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<DownloadItem> StorageManager::GetDownloads(int limit) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<DownloadItem> items;
    if (!db_) return items;

    const char* sql = "SELECT id, file_path, url, total_bytes, received_bytes, state, started_at, completed_at "
                      "FROM downloads ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return items;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DownloadItem item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.filePath = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
        item.url = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
        item.totalBytes = sqlite3_column_int64(stmt, 3);
        item.receivedBytes = sqlite3_column_int64(stmt, 4);
        item.state = Utf8ToWide((const char*)sqlite3_column_text(stmt, 5));
        item.startedAt = sqlite3_column_int64(stmt, 6);
        item.completedAt = sqlite3_column_int64(stmt, 7);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

std::wstring StorageManager::GetPreference(const std::wstring& key, const std::wstring& defaultValue) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return defaultValue;

    const char* sql = "SELECT value FROM preferences WHERE key = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return defaultValue;

    std::string u8Key = WideToUtf8(key);
    sqlite3_bind_text(stmt, 1, u8Key.c_str(), -1, SQLITE_STATIC);

    std::wstring result = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = Utf8ToWide((const char*)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return result;
}

bool StorageManager::SetPreference(const std::wstring& key, const std::wstring& value) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* sql = "INSERT INTO preferences (key, value) VALUES (?, ?) "
                      "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string u8Key = WideToUtf8(key);
    std::string u8Val = WideToUtf8(value);

    sqlite3_bind_text(stmt, 1, u8Key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, u8Val.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

} // namespace LiteBrowser
