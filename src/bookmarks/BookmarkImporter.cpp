#include "BookmarkImporter.h"
#include "../storage/StorageManager.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <regex>

namespace LiteBrowser {

namespace {

std::wstring Utf8ToWideStr(const std::string& u8) {
    if (u8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), &result[0], size);
    return result;
}

std::wstring GetLocalAppDataDir() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        return path;
    }
    return L"";
}

} // namespace

std::wstring BookmarkImporter::FindChromeBookmarksFile() {
    std::wstring localApp = GetLocalAppDataDir();
    if (localApp.empty()) return L"";
    std::wstring file = localApp + L"\\Google\\Chrome\\User Data\\Default\\Bookmarks";
    DWORD attr = GetFileAttributesW(file.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return file;
    }
    return L"";
}

std::wstring BookmarkImporter::FindEdgeBookmarksFile() {
    std::wstring localApp = GetLocalAppDataDir();
    if (localApp.empty()) return L"";
    std::wstring file = localApp + L"\\Microsoft\\Edge\\User Data\\Default\\Bookmarks";
    DWORD attr = GetFileAttributesW(file.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return file;
    }
    return L"";
}

bool BookmarkImporter::ImportChromeBookmarks(int& outImportedCount) {
    std::wstring path = FindChromeBookmarksFile();
    if (path.empty()) {
        outImportedCount = 0;
        return false;
    }
    return ImportFromFile(path, outImportedCount);
}

bool BookmarkImporter::ImportEdgeBookmarks(int& outImportedCount) {
    std::wstring path = FindEdgeBookmarksFile();
    if (path.empty()) {
        outImportedCount = 0;
        return false;
    }
    return ImportFromFile(path, outImportedCount);
}

bool BookmarkImporter::ImportFromFile(const std::wstring& filePath, int& outImportedCount) {
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        outImportedCount = 0;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return ParseAndInsertBookmarks(content, outImportedCount);
}

bool BookmarkImporter::ParseAndInsertBookmarks(const std::string& json, int& outImportedCount) {
    outImportedCount = 0;
    if (json.empty()) return false;

    // Fast JSON scanner for url-type bookmarks:
    // Chromium bookmark items contain: "name": "...", "type": "url", "url": "..."
    // We can regex or token match the key pairs
    std::regex itemRegex(R"raw(\{\s*"date_added"[^}]*?"name"\s*:\s*"([^"]+)"[^}]*?"type"\s*:\s*"url"[^}]*?"url"\s*:\s*"([^"]+)")raw");
    
    // Also try variant order
    std::regex variantRegex(R"raw("name"\s*:\s*"([^"]+)"[\s\S]*?"type"\s*:\s*"url"[\s\S]*?"url"\s*:\s*"([^"]+)")raw");

    auto words_begin = std::sregex_iterator(json.begin(), json.end(), itemRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string title = match[1].str();
        std::string url = match[2].str();

        if (!url.empty()) {
            std::wstring wUrl = Utf8ToWideStr(url);
            std::wstring wTitle = Utf8ToWideStr(title);

            if (!StorageManager::GetInstance().IsBookmarked(wUrl)) {
                StorageManager::GetInstance().AddBookmark(wUrl, wTitle.empty() ? wUrl : wTitle);
                outImportedCount++;
            }
        }
    }

    // If itemRegex found none, try variant
    if (outImportedCount == 0) {
        auto v_begin = std::sregex_iterator(json.begin(), json.end(), variantRegex);
        for (std::sregex_iterator i = v_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string title = match[1].str();
            std::string url = match[2].str();
            if (!url.empty() && url.rfind("http", 0) == 0) {
                std::wstring wUrl = Utf8ToWideStr(url);
                std::wstring wTitle = Utf8ToWideStr(title);
                if (!StorageManager::GetInstance().IsBookmarked(wUrl)) {
                    StorageManager::GetInstance().AddBookmark(wUrl, wTitle.empty() ? wUrl : wTitle);
                    outImportedCount++;
                }
            }
        }
    }

    return outImportedCount > 0;
}

} // namespace LiteBrowser
