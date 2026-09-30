#pragma once

#include <unknwn.h>
#include <utility>
#include <type_traits>
#include <atomic>
#include "WebView2.h"

namespace LiteBrowser {

// Lightweight RAII COM Smart Pointer
template <typename T>
class ComPtr {
public:
    ComPtr() noexcept : ptr_(nullptr) {}
    ComPtr(std::nullptr_t) noexcept : ptr_(nullptr) {}

    explicit ComPtr(T* p) noexcept : ptr_(p) {
        if (ptr_) ptr_->AddRef();
    }

    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) ptr_->AddRef();
    }

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ~ComPtr() noexcept {
        Reset();
    }

    ComPtr& operator=(std::nullptr_t) noexcept {
        Reset();
        return *this;
    }

    ComPtr& operator=(T* p) noexcept {
        if (ptr_ != p) {
            Reset();
            ptr_ = p;
            if (ptr_) ptr_->AddRef();
        }
        return *this;
    }

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->AddRef();
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T** GetAddressOf() noexcept {
        Reset();
        return &ptr_;
    }

    void** PutVoid() noexcept {
        Reset();
        return reinterpret_cast<void**>(&ptr_);
    }

    void Reset() noexcept {
        if (ptr_) {
            T* temp = ptr_;
            ptr_ = nullptr;
            temp->Release();
        }
    }

    T* Detach() noexcept {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    void Attach(T* p) noexcept {
        Reset();
        ptr_ = p;
    }

    template <typename Q>
    HRESULT As(ComPtr<Q>& out) const noexcept {
        if (!ptr_) return E_POINTER;
        return ptr_->QueryInterface(__uuidof(Q), reinterpret_cast<void**>(out.GetAddressOf()));
    }

private:
    T* ptr_;
};

// Generic 2-argument COM Handler Base
template <typename IInterface, typename A1, typename A2, typename F>
class ComHandlerBase2 : public IInterface {
public:
    explicit ComHandlerBase2(F&& callback) : refCount_(1), callback_(std::forward<F>(callback)) {}
    virtual ~ComHandlerBase2() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID /*riid*/, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        *ppvObject = static_cast<IInterface*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++refCount_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = --refCount_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(A1 a1, A2 a2) override {
        return callback_(a1, a2);
    }

private:
    std::atomic<ULONG> refCount_;
    F callback_;
};

// Primary template
template <typename IInterface, typename F>
class ComHandler;

#define DEFINE_HANDLER2(Iface, A1, A2) \
template <typename F> \
class ComHandler<Iface, F> final : public ComHandlerBase2<Iface, A1, A2, F> { \
public: \
    using ComHandlerBase2<Iface, A1, A2, F>::ComHandlerBase2; \
};

DEFINE_HANDLER2(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*)
DEFINE_HANDLER2(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*)
DEFINE_HANDLER2(ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*)
DEFINE_HANDLER2(ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2SourceChangedEventHandler, ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2DocumentTitleChangedEventHandler, ICoreWebView2*, IUnknown*)
DEFINE_HANDLER2(ICoreWebView2WebResourceRequestedEventHandler, ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2ProcessFailedEventHandler, ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2ExecuteScriptCompletedHandler, HRESULT, LPCWSTR)
DEFINE_HANDLER2(ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*)
DEFINE_HANDLER2(ICoreWebView2DownloadStartingEventHandler, ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs*)

#undef DEFINE_HANDLER2

template <typename IInterface, typename F>
ComPtr<IInterface> MakeCallback(F&& func) {
    return ComPtr<IInterface>(new ComHandler<IInterface, std::decay_t<F>>(std::forward<F>(func)));
}

} // namespace LiteBrowser
