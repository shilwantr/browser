#pragma once

#include <string>
#include <vector>

namespace LiteBrowser {

enum class ReaderTheme {
    Light,
    Warm, // Sepia
    Dark
};

enum class ReaderFont {
    Sans,
    Serif,
    Mono
};

struct ReaderPreferences {
    ReaderTheme theme = ReaderTheme::Warm;
    ReaderFont font = ReaderFont::Serif;
    int fontSize = 18; // in px (14 - 28)
    int contentWidth = 720; // in px (600 - 900)
    double lineHeight = 1.65; // (1.4 - 2.0)
    bool autoEnter = false;
};

struct ArticleMetadata {
    bool isArticle = false;
    int score = 0;
    std::wstring title;
    std::wstring byline;
    std::wstring author;
    std::wstring publishedDate;
    std::wstring heroImage;
    int readingTimeMinutes = 1;
    size_t wordCount = 0;
    std::wstring rawContentHtml;
    std::wstring sanitizedHtml;
    std::wstring originalUrl;
};

class ReaderModeEngine {
public:
    static ReaderModeEngine& GetInstance();

    ReaderModeEngine();
    ~ReaderModeEngine();

    // JavaScript code to evaluate article presence and extract clean content
    std::wstring GetDetectionScript() const;
    std::wstring GetExtractionScript() const;

    // C++ Content Sanitization & Security
    std::wstring SanitizeHtml(const std::wstring& rawHtml);
    int CalculateConfidenceScore(const ArticleMetadata& meta);

    // Document Generation
    std::wstring GenerateReaderHtml(const ArticleMetadata& meta, const ReaderPreferences& prefs);

    // Preferences
    ReaderPreferences GetPreferences() const;
    void SetPreferences(const ReaderPreferences& prefs);

private:
    ReaderPreferences currentPrefs_;
};

} // namespace LiteBrowser
