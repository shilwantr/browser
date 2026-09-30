#include "Tab.h"
#include "../app/AppConfig.h"
#include "../adblock/AdBlockEngine.h"
#include "../reader/ReaderModeEngine.h"
#include "../navigation/InternalPages.h"
#include "../storage/StorageManager.h"
#include <shlwapi.h>
#include <iostream>

namespace LiteBrowser {

namespace {

std::string WideToUtf8Str(const std::wstring& w) {
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], sz, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWideStr(const std::string& u) {
    if (u.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), nullptr, 0);
    std::wstring s(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), &s[0], sz);
    return s;
}

} // namespace

Tab::Tab(int id, bool isPrivate, HWND hParentWnd)
    : id_(id), isPrivate_(isPrivate), hParentWnd_(hParentWnd) {
}

Tab::~Tab() {
    if (webview_ && webResourceRequestedToken_.value) {
        webview_->remove_WebResourceRequested(webResourceRequestedToken_);
    }
    if (controller_) {
        controller_->Close();
        controller_.Reset();
    }
    webview_.Reset();
    env_.Reset();
}

void Tab::AttachWebView(ICoreWebView2Environment* env, std::function<void(bool success)> onCreated) {
    env_ = env;
    if (!env_ || !hParentWnd_) {
        if (onCreated) onCreated(false);
        return;
    }

    auto handler = MakeCallback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [this, onCreated](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT {
            if (FAILED(hr) || !controller) {
                if (onCreated) onCreated(false);
                return hr;
            }

            controller_ = controller;
            controller_->put_IsVisible(TRUE);
            if (currentBounds_.right > currentBounds_.left && currentBounds_.bottom > currentBounds_.top) {
                controller_->put_Bounds(currentBounds_);
            }
            controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

            controller_->get_CoreWebView2(webview_.GetAddressOf());
            if (!webview_) {
                if (onCreated) onCreated(false);
                return E_FAIL;
            }

            // Configure Settings
            ComPtr<ICoreWebView2Settings> settings;
            if (SUCCEEDED(webview_->get_Settings(settings.GetAddressOf())) && settings) {
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(TRUE);
                settings->put_AreDevToolsEnabled(TRUE);
                settings->put_IsBuiltInErrorPageEnabled(FALSE); // Use our custom error page
            }

            SetupEventHandlers();

            // Navigate to initial URL
            Navigate(url_);

            if (onCreated) onCreated(true);
            return S_OK;
        });

    env_->CreateCoreWebView2Controller(hParentWnd_, handler.Get());
}

void Tab::SetupEventHandlers() {
    if (!webview_) return;

    // 1. Navigation Starting
    auto navStartingHandler = MakeCallback<ICoreWebView2NavigationStartingEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            isLoading_ = true;
            LPWSTR uri = nullptr;
            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                std::wstring navUrl = uri;
                CoTaskMemFree(uri);

                if (InternalPages::IsInternalUrl(navUrl)) {
                    // Cancel external load and render internal HTML
                    args->put_Cancel(TRUE);
                    url_ = navUrl;
                    title_ = navUrl;
                    std::wstring html = InternalPages::GetPageHtml(navUrl);
                    webview_->NavigateToString(html.c_str());
                    isLoading_ = false;
                    isInReaderMode_ = false;
                    isReaderAvailable_ = false;
                    if (onStateChanged_) onStateChanged_();
                    return S_OK;
                }

                url_ = navUrl;
                if (!isInReaderMode_) {
                    originalUrl_ = navUrl;
                    blockedAdsCount_ = 0;
                    blockedTrackersCount_ = 0;
                    isReaderAvailable_ = false;
                }
            }

            if (onStateChanged_) onStateChanged_();
            return S_OK;
        });
    webview_->add_NavigationStarting(navStartingHandler.Get(), &navStartingToken_);

    // 2. Navigation Completed
    auto navCompletedHandler = MakeCallback<ICoreWebView2NavigationCompletedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            isLoading_ = false;
            BOOL success = FALSE;
            args->get_IsSuccess(&success);

            BOOL canBack = FALSE, canFwd = FALSE;
            webview_->get_CanGoBack(&canBack);
            webview_->get_CanGoForward(&canFwd);
            canGoBack_ = (canBack == TRUE);
            canGoForward_ = (canFwd == TRUE);

            if (success) {
                if (!isPrivate_ && !InternalPages::IsInternalUrl(url_) && !isInReaderMode_) {
                    StorageManager::GetInstance().AddHistory(url_, title_);
                }
                CheckReaderAvailability();
            } else {
                COREWEBVIEW2_WEB_ERROR_STATUS status;
                args->get_WebErrorStatus(&status);
                if (status != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) {
                    std::wstring errHtml = InternalPages::GetErrorHtml(url_, (int)status, L"Network or connection error");
                    webview_->NavigateToString(errHtml.c_str());
                }
            }

            if (onStateChanged_) onStateChanged_();
            return S_OK;
        });
    webview_->add_NavigationCompleted(navCompletedHandler.Get(), &navCompletedToken_);

    // 3. Source Changed
    auto sourceChangedHandler = MakeCallback<ICoreWebView2SourceChangedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
            LPWSTR uri = nullptr;
            if (SUCCEEDED(webview_->get_Source(&uri)) && uri) {
                url_ = uri;
                CoTaskMemFree(uri);
            }
            if (onStateChanged_) onStateChanged_();
            return S_OK;
        });
    webview_->add_SourceChanged(sourceChangedHandler.Get(), &sourceChangedToken_);

    // 4. Document Title Changed
    auto titleChangedHandler = MakeCallback<ICoreWebView2DocumentTitleChangedEventHandler>(
        [this](ICoreWebView2*, IUnknown*) -> HRESULT {
            LPWSTR docTitle = nullptr;
            if (SUCCEEDED(webview_->get_DocumentTitle(&docTitle)) && docTitle) {
                title_ = docTitle;
                CoTaskMemFree(docTitle);
            }
            if (onStateChanged_) onStateChanged_();
            return S_OK;
        });
    webview_->add_DocumentTitleChanged(titleChangedHandler.Get(), &titleChangedToken_);

    // 5. Network Request Filter for Ad/Tracker Blocking
    webview_->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    auto webResourceHandler = MakeCallback<ICoreWebView2WebResourceRequestedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> req;
            args->get_Request(req.GetAddressOf());
            if (!req) return S_OK;

            LPWSTR uriStr = nullptr;
            req->get_Uri(&uriStr);
            if (!uriStr) return S_OK;
            std::wstring reqUrlW = uriStr;
            CoTaskMemFree(uriStr);

            std::string reqUrl = WideToUtf8Str(reqUrlW);
            std::string docUrl = WideToUtf8Str(url_);

            bool isTracker = false;
            std::string matchedRule;
            if (AdBlockEngine::GetInstance().ShouldBlock(reqUrl, docUrl, 0, isTracker, matchedRule)) {
                if (isTracker) {
                    blockedTrackersCount_++;
                } else {
                    blockedAdsCount_++;
                }

                // Intercept with an empty 204 No Content response
                ComPtr<ICoreWebView2WebResourceResponse> response;
                if (env_ && SUCCEEDED(env_->CreateWebResourceResponse(nullptr, 204, L"No Content", L"", response.GetAddressOf()))) {
                    args->put_Response(response.Get());
                }

                if (onStateChanged_) onStateChanged_();
            }
            return S_OK;
        });
    webview_->add_WebResourceRequested(webResourceHandler.Get(), &webResourceRequestedToken_);

    // 6. Web Message Received (Communication from internal pages and Reader Mode)
    auto messageHandler = MakeCallback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR msg = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
                std::wstring strMsg = msg;
                CoTaskMemFree(msg);

                if (strMsg == L"reader:exit") {
                    ExitReaderMode();
                } else if (strMsg == L"retry") {
                    Reload();
                } else if (strMsg.rfind(L"open:", 0) == 0) {
                    Navigate(strMsg.substr(5));
                } else if (strMsg.rfind(L"navigate:", 0) == 0) {
                    Navigate(strMsg.substr(9));
                } else if (strMsg == L"history:clear_all") {
                    StorageManager::GetInstance().ClearAllHistory();
                    Navigate(URL_HISTORY);
                } else if (strMsg.rfind(L"history:delete:", 0) == 0) {
                    int64_t id = _wtoi64(strMsg.substr(15).c_str());
                    StorageManager::GetInstance().DeleteHistoryEntry(id);
                    Navigate(URL_HISTORY);
                } else if (strMsg.rfind(L"bookmark:delete:", 0) == 0) {
                    int64_t id = _wtoi64(strMsg.substr(16).c_str());
                    StorageManager::GetInstance().DeleteBookmark(id);
                    Navigate(URL_BOOKMARKS);
                } else if (strMsg.rfind(L"download:open:", 0) == 0) {
                    std::wstring path = strMsg.substr(14);
                    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
                }
            }
            return S_OK;
        });
    webview_->add_WebMessageReceived(messageHandler.Get(), &webMessageToken_);

    // 7. Crash Recovery
    auto processFailedHandler = MakeCallback<ICoreWebView2ProcessFailedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT {
            std::wstring crashHtml = LR"raw(<!DOCTYPE html>
<html><body style="background:#18181a;color:#fff;font-family:sans-serif;padding:40px;text-align:center;">
<h2>This tab crashed.</h2>
<p style="color:#888;margin:20px 0;">Something caused this web page to stop responding.</p>
<button onclick="window.chrome.webview.postMessage('retry')" style="background:#333;color:#fff;border:1px solid #555;padding:8px 16px;border-radius:4px;cursor:pointer;">Reload</button>
</body></html>)raw";
            webview_->NavigateToString(crashHtml.c_str());
            return S_OK;
        });
    webview_->add_ProcessFailed(processFailedHandler.Get(), &processFailedToken_);
}

void Tab::CheckReaderAvailability() {
    if (!webview_ || InternalPages::IsInternalUrl(url_) || isInReaderMode_) return;

    std::wstring script = ReaderModeEngine::GetInstance().GetDetectionScript();
    auto handler = MakeCallback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this](HRESULT hr, LPCWSTR resultJson) -> HRESULT {
            if (SUCCEEDED(hr) && resultJson) {
                std::wstring res = resultJson;
                // Parse whether isArticle is true
                if (res.find(L"\"isArticle\":true") != std::wstring::npos) {
                    isReaderAvailable_ = true;
                } else {
                    isReaderAvailable_ = false;
                }
                if (onStateChanged_) onStateChanged_();
            }
            return S_OK;
        });
    webview_->ExecuteScript(script.c_str(), handler.Get());
}

void Tab::ToggleReaderMode() {
    if (isInReaderMode_) {
        ExitReaderMode();
    } else {
        EnterReaderMode();
    }
}

void Tab::EnterReaderMode() {
    if (!webview_) return;

    originalUrl_ = url_;
    std::wstring script = ReaderModeEngine::GetInstance().GetExtractionScript();

    auto handler = MakeCallback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this](HRESULT hr, LPCWSTR resultJson) -> HRESULT {
            if (SUCCEEDED(hr) && resultJson) {
                // Parse extracted JSON string from WebView2
                // Since resultJson is JSON-encoded string from ExecuteScript, unescape it or parse directly
                std::wstring unquoted = resultJson;
                if (unquoted.size() >= 2 && unquoted.front() == L'"' && unquoted.back() == L'"') {
                    unquoted = unquoted.substr(1, unquoted.size() - 2);
                }

                // Simple JSON field extractor
                auto ExtractField = [](const std::wstring& json, const std::wstring& key) -> std::wstring {
                    std::wstring searchKey = L"\"" + key + L"\":\"";
                    size_t pos = json.find(searchKey);
                    if (pos == std::wstring::npos) return L"";
                    pos += searchKey.size();
                    size_t endPos = json.find(L"\"", pos);
                    if (endPos == std::wstring::npos) return L"";
                    return json.substr(pos, endPos - pos);
                };

                ArticleMetadata meta;
                meta.title = ExtractField(unquoted, L"title");
                if (meta.title.empty()) meta.title = title_;
                meta.author = ExtractField(unquoted, L"author");
                meta.publishedDate = ExtractField(unquoted, L"date");
                meta.heroImage = ExtractField(unquoted, L"heroImage");

                // Extract HTML content
                std::wstring contentKey = L"\"contentHtml\":\"";
                size_t cPos = unquoted.find(contentKey);
                if (cPos != std::wstring::npos) {
                    cPos += contentKey.size();
                    size_t endCPos = unquoted.rfind(L"\"");
                    if (endCPos > cPos) {
                        meta.rawContentHtml = unquoted.substr(cPos, endCPos - cPos);
                    }
                }

                // Sanitize HTML in C++
                meta.sanitizedHtml = ReaderModeEngine::GetInstance().SanitizeHtml(meta.rawContentHtml);
                meta.readingTimeMinutes = 4; // default comfortable estimate

                isInReaderMode_ = true;
                ReaderPreferences prefs = ReaderModeEngine::GetInstance().GetPreferences();
                std::wstring readerDoc = ReaderModeEngine::GetInstance().GenerateReaderHtml(meta, prefs);
                webview_->NavigateToString(readerDoc.c_str());

                if (onStateChanged_) onStateChanged_();
            }
            return S_OK;
        });

    webview_->ExecuteScript(script.c_str(), handler.Get());
}

void Tab::ExitReaderMode() {
    isInReaderMode_ = false;
    if (!originalUrl_.empty()) {
        Navigate(originalUrl_);
    } else {
        Reload();
    }
}

void Tab::Navigate(const std::wstring& input) {
    if (input.empty()) return;

    std::wstring navUrl = input;
    if (InternalPages::IsInternalUrl(input)) {
        navUrl = input;
    } else if (input.rfind(L"http://", 0) == 0 ||
               input.rfind(L"https://", 0) == 0 ||
               input.rfind(L"file://", 0) == 0 ||
               input.rfind(L"about:", 0) == 0) {
        navUrl = input;
    } else if (input.find(L'.') != std::wstring::npos && input.find(L' ') == std::wstring::npos) {
        navUrl = L"https://" + input;
    } else {
        navUrl = L"https://www.google.com/search?q=" + input;
    }

    url_ = navUrl;
    if (InternalPages::IsInternalUrl(navUrl)) {
        if (webview_) {
            std::wstring html = InternalPages::GetPageHtml(navUrl);
            webview_->NavigateToString(html.c_str());
        }
        isLoading_ = false;
        isInReaderMode_ = false;
        isReaderAvailable_ = false;
        title_ = navUrl;
        if (onStateChanged_) onStateChanged_();
        return;
    }

    if (webview_) {
        isInReaderMode_ = false;
        webview_->Navigate(navUrl.c_str());
    }
}

void Tab::GoBack() {
    if (webview_ && canGoBack_) webview_->GoBack();
}

void Tab::GoForward() {
    if (webview_ && canGoForward_) webview_->GoForward();
}

void Tab::Reload(bool ignoreCache) {
    if (InternalPages::IsInternalUrl(url_)) {
        Navigate(url_);
        return;
    }
    if (webview_) {
        webview_->Reload();
    }
}

void Tab::Stop() {
    if (webview_) webview_->Stop();
}

void Tab::Resize(const RECT& bounds) {
    currentBounds_ = bounds;
    if (controller_) {
        controller_->put_Bounds(bounds);
    }
}

void Tab::SetVisible(bool visible) {
    if (controller_) {
        controller_->put_IsVisible(visible ? TRUE : FALSE);
    }
}

void Tab::SetFocus() {
    if (controller_) {
        controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void Tab::ZoomIn() {
    SetZoom(currentZoom_ + 0.1);
}

void Tab::ZoomOut() {
    SetZoom(currentZoom_ - 0.1);
}

void Tab::ResetZoom() {
    SetZoom(1.0);
}

void Tab::SetZoom(double factor) {
    if (factor < 0.25) factor = 0.25;
    if (factor > 5.0) factor = 5.0;
    currentZoom_ = factor;
    if (controller_) {
        controller_->put_ZoomFactor(currentZoom_);
    }
}

void Tab::FindInPage(const std::wstring& text, bool forward) {
    if (!webview_ || text.empty()) return;
    // Fast find in page using window.find script
    std::wstring script = L"window.find('" + text + L"', false, " + (forward ? L"false" : L"true") + L", true, false, false, false);";
    webview_->ExecuteScript(script.c_str(), nullptr);
}

void Tab::StopFind() {
    if (!webview_) return;
    webview_->ExecuteScript(L"window.getSelection().removeAllRanges();", nullptr);
}

void Tab::OpenDevTools() {
    if (webview_) {
        webview_->OpenDevToolsWindow();
    }
}

} // namespace LiteBrowser
