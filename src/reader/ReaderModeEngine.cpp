#include "ReaderModeEngine.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace LiteBrowser {

ReaderModeEngine& ReaderModeEngine::GetInstance() {
    static ReaderModeEngine instance;
    return instance;
}

ReaderModeEngine::ReaderModeEngine() {
    currentPrefs_.theme = ReaderTheme::Warm;
    currentPrefs_.font = ReaderFont::Serif;
    currentPrefs_.fontSize = 19;
    currentPrefs_.contentWidth = 720;
    currentPrefs_.lineHeight = 1.68;
}

ReaderModeEngine::~ReaderModeEngine() = default;

ReaderPreferences ReaderModeEngine::GetPreferences() const {
    return currentPrefs_;
}

void ReaderModeEngine::SetPreferences(const ReaderPreferences& prefs) {
    currentPrefs_ = prefs;
}

std::wstring ReaderModeEngine::GetDetectionScript() const {
    // Fast in-page article scoring detector
    return LR"raw(
(() => {
    try {
        let score = 0;
        // 1. Semantic tags
        const article = document.querySelector('article');
        const main = document.querySelector('main');
        if (article) score += 35;
        if (main) score += 20;

        // 2. OpenGraph / Schema.org
        const ogType = document.querySelector('meta[property="og:type"]');
        if (ogType && ogType.content === 'article') score += 30;

        const jsonLd = document.querySelectorAll('script[type="application/ld+json"]');
        for (const el of jsonLd) {
            if (el.textContent && el.textContent.includes('"Article"')) {
                score += 35;
                break;
            }
        }

        // 3. Paragraph count and text length
        const paragraphs = document.querySelectorAll('p');
        let totalWords = 0;
        let pCount = 0;
        paragraphs.forEach(p => {
            const text = p.innerText || '';
            const words = text.trim().split(/\s+/).filter(w => w.length > 0).length;
            if (words > 15) {
                totalWords += words;
                pCount++;
            }
        });

        if (pCount >= 3) score += 20;
        if (totalWords >= 250) score += 25;
        if (totalWords >= 600) score += 20;

        // 4. Heading presence
        const h1 = document.querySelector('h1');
        if (h1 && (h1.innerText || '').length > 5) score += 15;

        // Return result
        return JSON.stringify({
            isArticle: score >= 50 && totalWords >= 200,
            score: score,
            wordCount: totalWords
        });
    } catch(e) {
        return JSON.stringify({ isArticle: false, score: 0, wordCount: 0 });
    }
})();
)raw";
}

std::wstring ReaderModeEngine::GetExtractionScript() const {
    // Comprehensive article extractor and sanitizer
    return LR"raw(
(() => {
    try {
        // 1. Metadata
        let title = '';
        const h1 = document.querySelector('h1');
        if (h1) title = h1.innerText.trim();
        if (!title) {
            const ogTitle = document.querySelector('meta[property="og:title"]');
            if (ogTitle) title = ogTitle.content.trim();
        }
        if (!title) title = document.title;

        // Author / Byline
        let author = '';
        const authorMeta = document.querySelector('meta[name="author"], meta[property="article:author"]');
        if (authorMeta) author = authorMeta.content.trim();
        if (!author) {
            const bylineEl = document.querySelector('[class*="author"], [class*="byline"], [rel="author"]');
            if (bylineEl) author = bylineEl.innerText.trim();
        }

        // Published Date
        let date = '';
        const dateMeta = document.querySelector('meta[property="article:published_time"], time[datetime]');
        if (dateMeta) {
            date = dateMeta.getAttribute('datetime') || dateMeta.content || (dateMeta.innerText ? dateMeta.innerText.trim() : '');
        }

        // Hero Image
        let heroImage = '';
        const ogImage = document.querySelector('meta[property="og:image"]');
        if (ogImage) heroImage = ogImage.content;

        // 2. Find best candidate root element
        let candidate = document.querySelector('article');
        if (!candidate) candidate = document.querySelector('main');
        if (!candidate) {
            // Find container with highest text-to-tag ratio
            let bestElem = null;
            let maxScore = 0;
            const divs = document.querySelectorAll('div, section');
            divs.forEach(el => {
                const pList = el.querySelectorAll('p');
                if (pList.length < 2) return;
                let words = 0;
                pList.forEach(p => { words += (p.innerText || '').split(/\s+/).length; });
                const links = el.querySelectorAll('a');
                let linkWords = 0;
                links.forEach(a => { linkWords += (a.innerText || '').split(/\s+/).length; });
                const score = words - (linkWords * 1.5);
                if (score > maxScore) {
                    maxScore = score;
                    bestElem = el;
                }
            });
            candidate = bestElem || document.body;
        }

        // Clone element to sanitize without altering current page
        const clone = candidate.cloneNode(true);

        // Remove junk elements
        const junkSelectors = [
            'script', 'style', 'iframe', 'noscript', 'canvas', 'svg',
            'nav', 'footer', 'header', 'aside', 'form', 'input', 'button',
            '[class*="comment"]', '[id*="comment"]',
            '[class*="sidebar"]', '[id*="sidebar"]',
            '[class*="advert"]', '[id*="advert"]',
            '[class*="share"]', '[class*="social"]',
            '[class*="newsletter"]', '[class*="subscribe"]',
            '[class*="related"]', '[id*="related"]',
            '[class*="popup"]', '[class*="modal"]'
        ];
        junkSelectors.forEach(sel => {
            clone.querySelectorAll(sel).forEach(el => el.remove());
        });

        // Strip attributes except safe ones (href, src, alt, title)
        const allElements = clone.querySelectorAll('*');
        allElements.forEach(el => {
            const allowed = ['href', 'src', 'alt', 'title', 'width', 'height'];
            const attrs = Array.from(el.attributes);
            attrs.forEach(attr => {
                const name = attr.name.toLowerCase();
                if (name.startsWith('on') || !allowed.includes(name)) {
                    el.removeAttribute(name);
                }
            });
            // Strip javascript: URLs
            if (el.hasAttribute('href')) {
                const h = el.getAttribute('href') || '';
                if (h.trim().toLowerCase().startsWith('javascript:')) {
                    el.removeAttribute('href');
                }
            }
        });

        // Calculate reading time
        const rawText = clone.innerText || '';
        const words = rawText.trim().split(/\s+/).filter(w => w.length > 0).length;
        const readingTimeMinutes = Math.max(1, Math.ceil(words / 200));

        return JSON.stringify({
            title: title,
            author: author,
            date: date,
            heroImage: heroImage,
            readingTimeMinutes: readingTimeMinutes,
            wordCount: words,
            contentHtml: clone.innerHTML
        });
    } catch(e) {
        return JSON.stringify({ error: e.toString() });
    }
})();
)raw";
}

std::wstring ReaderModeEngine::SanitizeHtml(const std::wstring& rawHtml) {
    if (rawHtml.empty()) return L"";

    // Aggressive C++ sanitization regex pass
    std::wstring result = rawHtml;

    // 1. Remove dangerous tags and their contents: <script>...</script>, <style>...</style>, <iframe>...</iframe>, <form>...</form>
    const std::wregex dangerousTags(L"<(script|style|iframe|object|embed|form|input|button|svg|canvas)[^>]*?>[\\s\\S]*?<\\/\\1>", std::regex_constants::icase);
    result = std::regex_replace(result, dangerousTags, L"");

    // 2. Remove self-closing or lone dangerous tags: <script.../>, <input...>, etc.
    const std::wregex loneTags(L"<(script|style|iframe|object|embed|form|input|button)[^>]*?>", std::regex_constants::icase);
    result = std::regex_replace(result, loneTags, L"");

    // 3. Remove inline event handlers (onclick=..., onload=..., etc.)
    const std::wregex eventHandlers(L"\\s+on[a-zA-Z]+\\s*=\\s*(\"[^\"]*\"|'[^']*'|[^\\s>]+)", std::regex_constants::icase);
    result = std::regex_replace(result, eventHandlers, L"");

    // 4. Remove javascript: pseudo-protocol
    const std::wregex jsHrefs(L"href\\s*=\\s*[\"']\\s*javascript:[^\"']*[\"']", std::regex_constants::icase);
    result = std::regex_replace(result, jsHrefs, L"href=\"#\"");

    return result;
}

int ReaderModeEngine::CalculateConfidenceScore(const ArticleMetadata& meta) {
    int score = 0;
    if (!meta.title.empty()) score += 20;
    if (!meta.author.empty()) score += 15;
    if (!meta.publishedDate.empty()) score += 15;
    if (meta.wordCount >= 250) score += 20;
    if (meta.wordCount >= 600) score += 20;
    if (!meta.heroImage.empty()) score += 10;
    return score;
}

std::wstring ReaderModeEngine::GenerateReaderHtml(const ArticleMetadata& meta, const ReaderPreferences& prefs) {
    std::wstringstream ss;

    // Determine Theme Colors
    std::wstring bg = L"#ffffff";
    std::wstring fg = L"#1a1a1a";
    std::wstring fgSec = L"#666666";
    std::wstring borderCol = L"#e5e5e5";
    std::wstring barBg = L"rgba(255, 255, 255, 0.95)";

    if (prefs.theme == ReaderTheme::Warm) {
        bg = L"#fbf7ee";
        fg = L"#2c2523";
        fgSec = L"#7c7068";
        borderCol = L"#e8decb";
        barBg = L"rgba(251, 247, 238, 0.95)";
    } else if (prefs.theme == ReaderTheme::Dark) {
        bg = L"#19191a";
        fg = L"#e3e3e3";
        fgSec = L"#999999";
        borderCol = L"#333333";
        barBg = L"rgba(25, 25, 26, 0.95)";
    }

    // Determine Font
    std::wstring fontFamily = L"'Segoe UI Variable Text', -apple-system, sans-serif";
    if (prefs.font == ReaderFont::Serif) {
        fontFamily = L"Georgia, 'Times New Roman', serif";
    } else if (prefs.font == ReaderFont::Mono) {
        fontFamily = L"Consolas, 'Courier New', monospace";
    }

    ss << L"<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n";
    ss << L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    ss << L"<title>" << meta.title << L" - Reader Mode</title>\n";
    ss << L"<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src https: http: data:; style-src 'unsafe-inline'; font-src data:;\">\n";
    ss << L"<style>\n";
    ss << L":root {\n";
    ss << L"  --bg: " << bg << L";\n";
    ss << L"  --fg: " << fg << L";\n";
    ss << L"  --fg-sec: " << fgSec << L";\n";
    ss << L"  --border: " << borderCol << L";\n";
    ss << L"  --bar-bg: " << barBg << L";\n";
    ss << L"}\n";
    ss << L"* { box-sizing: border-box; margin: 0; padding: 0; }\n";
    ss << L"body {\n";
    ss << L"  background-color: var(--bg);\n";
    ss << L"  color: var(--fg);\n";
    ss << L"  font-family: " << fontFamily << L";\n";
    ss << L"  font-size: " << prefs.fontSize << L"px;\n";
    ss << L"  line-height: " << prefs.lineHeight << L";\n";
    ss << L"  transition: background 0.15s ease, color 0.15s ease;\n";
    ss << L"}\n";
    ss << L"#reader-bar {\n";
    ss << L"  position: sticky; top: 0; z-index: 100;\n";
    ss << L"  background: var(--bar-bg); backdrop-filter: blur(10px);\n";
    ss << L"  border-bottom: 1px solid var(--border);\n";
    ss << L"  display: flex; justify-content: space-between; align-items: center;\n";
    ss << L"  padding: 8px 24px; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; font-size: 13px;\n";
    ss << L"}\n";
    ss << L".reader-actions { display: flex; align-items: center; gap: 12px; }\n";
    ss << L".btn {\n";
    ss << L"  background: transparent; border: 1px solid var(--border); border-radius: 4px;\n";
    ss << L"  color: var(--fg); padding: 4px 10px; cursor: pointer; font-size: 12px;\n";
    ss << L"}\n";
    ss << L".btn:hover { background: var(--border); }\n";
    ss << L"#container {\n";
    ss << L"  max-width: " << prefs.contentWidth << L"px;\n";
    ss << L"  margin: 40px auto 100px auto;\n";
    ss << L"  padding: 0 24px;\n";
    ss << L"}\n";
    ss << L"h1.article-title {\n";
    ss << L"  font-size: 2.2em; font-weight: 700; line-height: 1.25; margin-bottom: 16px;\n";
    ss << L"}\n";
    ss << L".article-meta {\n";
    ss << L"  color: var(--fg-sec); font-size: 0.9em; margin-bottom: 32px;\n";
    ss << L"  display: flex; flex-wrap: wrap; gap: 12px; align-items: center;\n";
    ss << L"}\n";
    ss << L".hero-img { width: 100%; border-radius: 6px; margin-bottom: 32px; object-fit: cover; max-height: 480px; }\n";
    ss << L"p { margin-bottom: 1.5em; }\n";
    ss << L"h2, h3, h4 { margin-top: 1.8em; margin-bottom: 0.8em; line-height: 1.3; }\n";
    ss << L"img { max-width: 100%; height: auto; border-radius: 4px; margin: 1.5em 0; }\n";
    ss << L"blockquote { border-left: 3px solid var(--border); padding-left: 18px; margin: 1.5em 0; font-style: italic; color: var(--fg-sec); }\n";
    ss << L"ul, ol { margin: 1.5em 0; padding-left: 28px; }\n";
    ss << L"li { margin-bottom: 0.5em; }\n";
    ss << L"pre, code { font-family: Consolas, monospace; background: var(--border); border-radius: 4px; font-size: 0.9em; }\n";
    ss << L"pre { padding: 14px; overflow-x: auto; margin: 1.5em 0; }\n";
    ss << L"code { padding: 2px 5px; }\n";
    ss << L"a { color: var(--fg); text-decoration: underline; text-underline-offset: 3px; }\n";
    ss << L"</style>\n</head>\n<body>\n";

    // Reader Bar
    ss << L"<div id=\"reader-bar\">\n";
    ss << L"  <div><span>📖 Reader</span> &bull; <span>" << meta.readingTimeMinutes << L" min read</span> (" << meta.wordCount << L" words)</div>\n";
    ss << L"  <div class=\"reader-actions\">\n";
    ss << L"    <button class=\"btn\" onclick=\"window.chrome.webview.postMessage('reader:exit')\">Exit Reader</button>\n";
    ss << L"  </div>\n";
    ss << L"</div>\n";

    // Container
    ss << L"<div id=\"container\">\n";
    ss << L"  <h1 class=\"article-title\">" << meta.title << L"</h1>\n";

    // Metadata line
    ss << L"  <div class=\"article-meta\">\n";
    if (!meta.author.empty()) {
        ss << L"    <span>By " << meta.author << L"</span>\n";
    }
    if (!meta.publishedDate.empty()) {
        ss << L"    <span>&bull;</span><span>" << meta.publishedDate << L"</span>\n";
    }
    ss << L"  </div>\n";

    // Hero Image
    if (!meta.heroImage.empty()) {
        ss << L"  <img class=\"hero-img\" src=\"" << meta.heroImage << L"\" alt=\"Article image\" />\n";
    }

    // Body Content
    ss << L"  <div class=\"article-body\">\n";
    ss << meta.sanitizedHtml;
    ss << L"  </div>\n";
    ss << L"</div>\n";

    ss << L"</body>\n</html>";
    return ss.str();
}

} // namespace LiteBrowser
