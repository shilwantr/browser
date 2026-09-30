#pragma once

#include <string>
#include <vector>

namespace LiteBrowser {

class BookmarkImporter {
public:
    static bool ImportChromeBookmarks(int& outImportedCount);
    static bool ImportEdgeBookmarks(int& outImportedCount);
    static bool ImportFromFile(const std::wstring& filePath, int& outImportedCount);

private:
    static std::wstring FindChromeBookmarksFile();
    static std::wstring FindEdgeBookmarksFile();
    static bool ParseAndInsertBookmarks(const std::string& jsonContent, int& outImportedCount);
};

} // namespace LiteBrowser
