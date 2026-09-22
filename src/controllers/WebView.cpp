#include <XBase/WebView.h>

#include <XBase/Hooks.h>
#include <XBase/Log.h>
#include <XBase/Platform.h>

#include "HooksInternal.h"
#include "InputInternal.h"
#include "webview2/WebView2.h"

#include <Windows.h>
#include <objbase.h>
#include <objidl.h>

#include "imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")

namespace {

using XBase::Rect;

using CreateEnvironmentWithOptionsFn = HRESULT(STDAPICALLTYPE*)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);
using GetBrowserVersionStringFn = HRESULT(STDAPICALLTYPE*)(
    PCWSTR browserExecutableFolder,
    LPWSTR* versionInfo);

constexpr const wchar_t* kHostWindowClass = L"XBaseWebViewHost";

// 抓帧模式参数：空闲与交互时的抓帧间隔、全屏状态检测间隔、交互判定阈值
constexpr unsigned long long kCaptureIdleIntervalMs = 500;
constexpr unsigned long long kCaptureActiveIntervalMs = 120;
constexpr unsigned long long kCaptureActiveWindowMs = 1500;
constexpr unsigned long long kCaptureModeCheckMs = 250;
constexpr unsigned long long kMoveForwardIntervalMs = 60;
constexpr float kInteractionMoveThreshold = 8.0f;

struct Runtime {
    HMODULE loader = nullptr;
    CreateEnvironmentWithOptionsFn createEnvironment = nullptr;
    GetBrowserVersionStringFn getVersion = nullptr;
};

struct WebViewState {
    std::mutex mutex;
    HWND gameWindow = nullptr;
    HWND hostWindow = nullptr;
    ICoreWebView2Environment* environment = nullptr;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    EventRegistrationToken navigationStartingToken{};
    EventRegistrationToken navigationCompletedToken{};
    EventRegistrationToken documentTitleToken{};
    EventRegistrationToken newWindowToken{};
    EventRegistrationToken acceleratorKeyToken{};
    bool tokensRegistered = false;
    bool acceleratorKeyRegistered = false;

    bool initRequested = false;
    bool createRequested = false;
    bool createInFlight = false;
    bool shutdownPending = false;
    bool initialized = false;
    bool visible = false;
    bool loading = false;
    bool canGoBack = false;
    bool canGoForward = false;
    int lastError = 0;
    int runtimeState = -1;  // -1 未检测，0 不可用，1 可用
    std::string url;
    std::string title;
    std::string pendingUrl;
    std::string pendingHtml;
    Rect bounds{};
    bool boundsApplied = false;
    float zoom = 1.0f;
    XBase::WebView::StateCallback stateCallback = nullptr;
    XBase::WebView::MessageHandler messageHandler = nullptr;
    std::vector<std::string> pendingScripts;
    EventRegistrationToken webMessageToken{};
    bool webMessageRegistered = false;

    // 独占全屏下改用抓帧贴图呈现
    bool previewReady = false;
    bool captureMode = false;
    bool menuWasVisible = false;
    IDirect3DTexture9* texture = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
    IStream* captureStream = nullptr;
    bool captureInFlight = false;
unsigned long long lastCaptureAt = 0;
unsigned long long lastInteractionAt = 0;
unsigned long long lastModeCheckAt = 0;
unsigned long long lastForwardedMoveAt = 0;
float lastForwardedMoveX = 0.0f;
float lastForwardedMoveY = 0.0f;
bool forwardedMoveValid = false;
bool previousMouseDown = false;
int cursorShows = 0;
};

Runtime s_runtime;
WebViewState s_state;

LRESULT CALLBACK HostWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void DestroyHostWindow();

std::string ModuleFilePath(const char* fileName) {
    return XBase::Platform::CurrentModuleDirectory() + fileName;
}

bool LoadRuntime() {
    if (s_runtime.loader) return true;

    const std::wstring localPath = XBase::Platform::Utf8ToWide(ModuleFilePath("WebView2Loader.dll"));
    HMODULE loader = LoadLibraryW(localPath.c_str());
    if (!loader) {
        loader = LoadLibraryW(L"WebView2Loader.dll");
    }
    if (!loader) {
        XBase::Log::Warn("WebView: WebView2Loader.dll 未找到，网页视图不可用");
        return false;
    }

    s_runtime.loader = loader;
    s_runtime.createEnvironment = reinterpret_cast<CreateEnvironmentWithOptionsFn>(
        GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
    s_runtime.getVersion = reinterpret_cast<GetBrowserVersionStringFn>(
        GetProcAddress(loader, "GetAvailableCoreWebView2BrowserVersionString"));
    if (!s_runtime.createEnvironment || !s_runtime.getVersion) {
        XBase::Log::Warn("WebView: WebView2Loader 缺少所需导出函数");
        return false;
    }
    return true;
}

std::wstring WideFrom(const std::string& value) {
    return XBase::Platform::Utf8ToWide(value);
}

std::string Utf8FromCoTaskMem(LPWSTR value) {
    if (!value) return {};
    std::string result = XBase::Platform::WideToUtf8(value);
    CoTaskMemFree(value);
    return result;
}

std::string SourceOf(ICoreWebView2* webview) {
    if (!webview) return {};
    LPWSTR source = nullptr;
    if (FAILED(webview->get_Source(&source))) return {};
    return Utf8FromCoTaskMem(source);
}

std::string TitleOf(ICoreWebView2* webview) {
    if (!webview) return {};
    LPWSTR title = nullptr;
    if (FAILED(webview->get_DocumentTitle(&title))) return {};
    return Utf8FromCoTaskMem(title);
}

void RefreshHistoryFlags(ICoreWebView2* webview, bool& canGoBack, bool& canGoForward) {
    canGoBack = false;
    canGoForward = false;
    if (!webview) return;
    BOOL value = FALSE;
    canGoBack = SUCCEEDED(webview->get_CanGoBack(&value)) && value != FALSE;
    value = FALSE;
    canGoForward = SUCCEEDED(webview->get_CanGoForward(&value)) && value != FALSE;
}

void NotifyStateChanged() {
    XBase::WebView::StateCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        callback = s_state.stateCallback;
    }
    if (callback) {
        callback();
    }
}

void ApplyBoundsLocked() {
    if (!s_state.hostWindow || !s_state.gameWindow) return;

    RECT client{};
    if (!GetClientRect(s_state.gameWindow, &client)) return;

    float displayWidth = 0.0f;
    float displayHeight = 0.0f;
    XBase::Detail::Hooks::GetDisplaySize(displayWidth, displayHeight);

    // 宿主使用 ImGui 显示坐标传入矩形，按客户区比例换算成子窗口坐标
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (displayWidth > 1.0f && displayHeight > 1.0f && client.right > 0 && client.bottom > 0) {
        scaleX = static_cast<float>(client.right) / displayWidth;
        scaleY = static_cast<float>(client.bottom) / displayHeight;
    }

    int left = static_cast<int>(s_state.bounds.left * scaleX);
    int top = static_cast<int>(s_state.bounds.top * scaleY);
    int width = static_cast<int>((s_state.bounds.right - s_state.bounds.left) * scaleX);
    int height = static_cast<int>((s_state.bounds.bottom - s_state.bounds.top) * scaleY);
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    if (left + width > client.right) width = client.right - left;
    if (top + height > client.bottom) height = client.bottom - top;
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    SetWindowPos(
        s_state.hostWindow, nullptr,
        left, top, width, height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    if (s_state.controller) {
        RECT bounds{0, 0, width, height};
        s_state.controller->put_Bounds(bounds);
    }
    s_state.boundsApplied = true;

    static RECT lastLogged{};
    if (client.right != lastLogged.right || client.bottom != lastLogged.bottom
        || left != lastLogged.left || top != lastLogged.top
        || width != (lastLogged.right - lastLogged.left)
        || height != (lastLogged.bottom - lastLogged.top)) {
        lastLogged.left = left;
        lastLogged.top = top;
        lastLogged.right = left + width;
        lastLogged.bottom = top + height;
        char message[192]{};
        std::snprintf(message, sizeof(message),
            "WebView bounds client=%dx%d display=%.0fx%.0f rect=%d,%d %dx%d",
            static_cast<int>(client.right), static_cast<int>(client.bottom),
            displayWidth, displayHeight, left, top, width, height);
        XBase::Log::Info(message);
    }
}

// 独占全屏下 DWM 不合成子窗口，HWND 覆盖层永远不可见
bool IsExclusiveFullscreen() {
    IDirect3DDevice9* device = XBase::Detail::Hooks::GetD3D9Device();
    if (!device) return false;
    IDirect3DSwapChain9* chain = nullptr;
    if (FAILED(device->GetSwapChain(0, &chain)) || !chain) return false;
    D3DPRESENT_PARAMETERS parameters{};
    const bool exclusive = SUCCEEDED(chain->GetPresentParameters(&parameters)) && !parameters.Windowed;
    chain->Release();
    return exclusive;
}

void ApplyVisibleLocked() {
    if (!s_state.hostWindow) return;
    // 独占全屏下抓帧模式不接受焦点，否则游戏会失去键盘输入
    const bool captureMode = IsExclusiveFullscreen();
    if (s_state.controller) {
        s_state.controller->put_IsVisible(s_state.visible ? TRUE : FALSE);
    }
    ShowWindow(s_state.hostWindow, s_state.visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (s_state.visible) {
        if (!captureMode) {
            SetFocus(s_state.hostWindow);
            if (s_state.controller) {
                s_state.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
        } else if (s_state.gameWindow) {
            SetFocus(s_state.gameWindow);
        }
    } else if (s_state.gameWindow) {
        SetFocus(s_state.gameWindow);
    }
}

void ApplyZoomLocked() {
    if (s_state.controller) {
        s_state.controller->put_ZoomFactor(static_cast<double>(s_state.zoom));
    }
}

// 调用方需持有 s_state.mutex；异步创建结束后才允许销毁宿主窗口
void ReleaseControllerLocked() {
    if (s_state.webview && s_state.tokensRegistered) {
        s_state.webview->remove_NavigationStarting(s_state.navigationStartingToken);
        s_state.webview->remove_NavigationCompleted(s_state.navigationCompletedToken);
        s_state.webview->remove_DocumentTitleChanged(s_state.documentTitleToken);
        s_state.webview->remove_NewWindowRequested(s_state.newWindowToken);
        if (s_state.webMessageRegistered) {
            s_state.webview->remove_WebMessageReceived(s_state.webMessageToken);
            s_state.webMessageRegistered = false;
        }
        s_state.tokensRegistered = false;
    }
    if (s_state.controller) {
        if (s_state.acceleratorKeyRegistered) {
            s_state.controller->remove_AcceleratorKeyPressed(s_state.acceleratorKeyToken);
            s_state.acceleratorKeyRegistered = false;
        }
        s_state.controller->Close();
        s_state.controller->Release();
        s_state.controller = nullptr;
    }
    if (s_state.webview) {
        s_state.webview->Release();
        s_state.webview = nullptr;
    }
    if (s_state.environment) {
        s_state.environment->Release();
        s_state.environment = nullptr;
    }
    if (s_state.captureStream) {
        s_state.captureStream->Release();
        s_state.captureStream = nullptr;
    }
    if (s_state.texture) {
        s_state.texture->Release();
        s_state.texture = nullptr;
    }
    s_state.captureInFlight = false;
    s_state.previewReady = false;
    s_state.textureWidth = 0;
    s_state.textureHeight = 0;
    s_state.previousMouseDown = false;
    DestroyHostWindow();
    s_state.initialized = false;
    s_state.loading = false;
    s_state.canGoBack = false;
    s_state.canGoForward = false;
    s_state.boundsApplied = false;
}

void ExecuteScript(const std::string& script) {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (!s_state.webview) return;
    s_state.webview->ExecuteScript(WideFrom(script).c_str(), nullptr);
}

bool DecodeImageToBgra(const std::vector<unsigned char>& data, unsigned char*& pixels, int& width, int& height) {
    if (data.empty() || data.size() > static_cast<std::size_t>(INT_MAX)) return false;

    int channels = 0;
    unsigned char* decoded = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
    if (!decoded || width <= 0 || height <= 0) {
        if (decoded) stbi_image_free(decoded);
        width = 0;
        height = 0;
        return false;
    }

    // stb 输出 RGBA，D3D9 纹理需要 BGRA；就地交换后直接上传，避免额外拷贝
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    for (std::size_t index = 0; index < pixelCount; ++index) {
        std::swap(decoded[index * 4], decoded[index * 4 + 2]);
        decoded[index * 4 + 3] = 255;
    }
    pixels = decoded;
    return true;
}

bool UploadTextureLocked(const unsigned char* pixels, int width, int height) {
    IDirect3DDevice9* device = XBase::Detail::Hooks::GetD3D9Device();
    if (!device || !pixels || width <= 0 || height <= 0) return false;

    if (!s_state.texture || s_state.textureWidth != width || s_state.textureHeight != height) {
        if (s_state.texture) {
            s_state.texture->Release();
            s_state.texture = nullptr;
        }
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(device->CreateTexture(
                static_cast<UINT>(width), static_cast<UINT>(height), 1,
                0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)) || !texture) {
            return false;
        }
        s_state.texture = texture;
        s_state.textureWidth = width;
        s_state.textureHeight = height;
    }

    D3DLOCKED_RECT locked{};
    if (FAILED(s_state.texture->LockRect(0, &locked, nullptr, 0))) {
        s_state.texture->Release();
        s_state.texture = nullptr;
        s_state.textureWidth = 0;
        s_state.textureHeight = 0;
        s_state.previewReady = false;
        return false;
    }
    for (int row = 0; row < height; ++row) {
        std::memcpy(
            static_cast<unsigned char*>(locked.pBits) + static_cast<std::size_t>(row) * locked.Pitch,
            pixels + static_cast<std::size_t>(row) * width * 4,
            static_cast<std::size_t>(width) * 4);
    }
    s_state.texture->UnlockRect(0);
    s_state.previewReady = true;
    return true;
}

void StartCapture();

#define XBASE_WEBVIEW_HANDLER_BODY(InterfaceName)                                      \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {    \
        if (!object) return E_POINTER;                                                 \
        *object = nullptr;                                                             \
        if (riid == IID_IUnknown || riid == __uuidof(InterfaceName)) {                 \
            *object = static_cast<InterfaceName*>(this);                               \
            AddRef();                                                                  \
            return S_OK;                                                               \
        }                                                                              \
        return E_NOINTERFACE;                                                          \
    }                                                                                  \
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount_; }                  \
    ULONG STDMETHODCALLTYPE Release() override {                                       \
        const ULONG remaining = --refCount_;                                           \
        if (remaining == 0) delete this;                                               \
        return remaining;                                                              \
    }                                                                                  \
    std::atomic<ULONG> refCount_{1};

class CaptureCompletedHandler final : public ICoreWebView2CapturePreviewCompletedHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2CapturePreviewCompletedHandler)

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode) override {
        std::vector<unsigned char> data;
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            IStream* stream = s_state.captureStream;
            s_state.captureStream = nullptr;
            s_state.captureInFlight = false;
            s_state.lastCaptureAt = static_cast<unsigned long long>(XBase::Platform::MonotonicMilliseconds());

            if (stream && SUCCEEDED(errorCode)) {
                STATSTG stat{};
                if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME))) {
                    const ULONGLONG size = stat.cbSize.QuadPart;
                    if (size > 0 && size < 64ull * 1024ull * 1024ull) {
                        data.resize(static_cast<std::size_t>(size));
                        LARGE_INTEGER origin{};
                        stream->Seek(origin, STREAM_SEEK_SET, nullptr);
                        ULONG read = 0;
                        if (FAILED(stream->Read(data.data(), static_cast<ULONG>(size), &read)) || read != size) {
                            data.clear();
                        }
                    }
                }
            }
            if (stream) stream->Release();
        }

        if (!data.empty()) {
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            if (DecodeImageToBgra(data, pixels, width, height)) {
                std::lock_guard<std::mutex> lock(s_state.mutex);
                UploadTextureLocked(pixels, width, height);
                stbi_image_free(pixels);
            }
        }
        return S_OK;
    }
};

void StartCapture() {
    ICoreWebView2* webview = nullptr;
    bool recentlyInteracted = false;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        if (s_state.captureInFlight || !s_state.webview || !s_state.visible) return;
        webview = s_state.webview;
        webview->AddRef();
        s_state.captureInFlight = true;
        const unsigned long long now = static_cast<unsigned long long>(XBase::Platform::MonotonicMilliseconds());
        recentlyInteracted = now - s_state.lastInteractionAt < kCaptureActiveWindowMs;
    }

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) || !stream) {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        s_state.captureInFlight = false;
        webview->Release();
        return;
    }

    auto* handler = new CaptureCompletedHandler();
    // 静止时用 PNG 保证清晰度，交互时用 JPEG 压低抓帧开销
    const COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT format =
        recentlyInteracted ? COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_JPEG
                           : COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG;
    const HRESULT hr = webview->CapturePreview(format, stream, handler);
    handler->Release();
    webview->Release();

    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (FAILED(hr)) {
        stream->Release();
        s_state.captureInFlight = false;
        return;
    }
    s_state.captureStream = stream;
}

// 网页子窗口持有焦点时按键不会到达游戏窗口，转发给 XBase 输入系统以便菜单热键继续工作
class AcceleratorKeyPressedHandler final : public ICoreWebView2AcceleratorKeyPressedEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2AcceleratorKeyPressedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args) override {
        if (!args) return S_OK;
        COREWEBVIEW2_KEY_EVENT_KIND kind = COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN;
        if (FAILED(args->get_KeyEventKind(&kind))) return S_OK;
        UINT virtualKey = 0;
        if (FAILED(args->get_VirtualKey(&virtualKey))) return S_OK;

        const bool down = kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN
            || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN;
        XBase::Detail::Input::HandleVirtualKey(static_cast<std::uint32_t>(virtualKey), down, false);
        return S_OK;
    }
};

class NavigationStartingHandler final : public ICoreWebView2NavigationStartingEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2NavigationStartingEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) override {
        std::string url;
        if (args) {
            LPWSTR uri = nullptr;
            if (SUCCEEDED(args->get_Uri(&uri))) {
                url = Utf8FromCoTaskMem(uri);
            }
        }
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            s_state.loading = true;
            s_state.lastError = 0;
            if (!url.empty()) s_state.url = url;
        }
        NotifyStateChanged();
        return S_OK;
    }
};

class NavigationCompletedHandler final : public ICoreWebView2NavigationCompletedEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2NavigationCompletedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        bool canGoBack = false;
        bool canGoForward = false;
        RefreshHistoryFlags(sender, canGoBack, canGoForward);
        const std::string url = SourceOf(sender);

        int error = 0;
        if (args) {
            COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
            if (SUCCEEDED(args->get_WebErrorStatus(&status))) {
                error = static_cast<int>(status);
            }
        }

        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            s_state.loading = false;
            s_state.canGoBack = canGoBack;
            s_state.canGoForward = canGoForward;
            s_state.lastError = error;
            if (!url.empty()) s_state.url = url;
        }
        NotifyStateChanged();
        return S_OK;
    }
};

class DocumentTitleChangedHandler final : public ICoreWebView2DocumentTitleChangedEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2DocumentTitleChangedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, IUnknown*) override {
        const std::string title = TitleOf(sender);
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            s_state.title = title;
        }
        NotifyStateChanged();
        return S_OK;
    }
};

// 网页用 postMessage 发来的 JSON 原样转给宿主注册的处理函数
class WebMessageReceivedHandler final : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2WebMessageReceivedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (!args) return S_OK;

        std::string message;
        LPWSTR json = nullptr;
        if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
            message = Utf8FromCoTaskMem(json);
        }
        if (message.empty()) return S_OK;

        XBase::WebView::MessageHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            handler = s_state.messageHandler;
        }
        if (handler) {
            handler(message);
        }
        return S_OK;
    }
};

class NewWindowRequestedHandler final : public ICoreWebView2NewWindowRequestedEventHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2NewWindowRequestedEventHandler)

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) override {
        if (!sender || !args) return S_OK;
        args->put_Handled(TRUE);
        LPWSTR uri = nullptr;
        if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
            sender->Navigate(uri);
            CoTaskMemFree(uri);
        }
        return S_OK;
    }
};

class ControllerCompletedHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override {
        bool ready = false;
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            s_state.createInFlight = false;

            if (s_state.shutdownPending || !s_state.hostWindow || !IsWindow(s_state.hostWindow)) {
                s_state.shutdownPending = false;
                if (controller) {
                    controller->Close();
                }
                ReleaseControllerLocked();
            } else if (FAILED(errorCode) || !controller) {
                XBase::Log::Error("WebView: WebView2 控制器创建失败");
            } else {
                s_state.controller = controller;
                s_state.controller->AddRef();
                s_state.controller->put_IsVisible(FALSE);
                ApplyBoundsLocked();
                ApplyZoomLocked();

                auto* acceleratorKey = new AcceleratorKeyPressedHandler();
                if (SUCCEEDED(s_state.controller->add_AcceleratorKeyPressed(
                        acceleratorKey, &s_state.acceleratorKeyToken))) {
                    s_state.acceleratorKeyRegistered = true;
                }
                acceleratorKey->Release();

                ICoreWebView2* webview = nullptr;
                if (SUCCEEDED(s_state.controller->get_CoreWebView2(&webview)) && webview) {
                    s_state.webview = webview;
                    ICoreWebView2Settings* settings = nullptr;
                    if (SUCCEEDED(webview->get_Settings(&settings)) && settings) {
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(TRUE);
                        settings->Release();
                    }

                    auto* navigationStarting = new NavigationStartingHandler();
                    auto* navigationCompleted = new NavigationCompletedHandler();
                    auto* titleChanged = new DocumentTitleChangedHandler();
                    auto* newWindow = new NewWindowRequestedHandler();
                    if (SUCCEEDED(webview->add_NavigationStarting(navigationStarting, &s_state.navigationStartingToken))
                        && SUCCEEDED(webview->add_NavigationCompleted(navigationCompleted, &s_state.navigationCompletedToken))
                        && SUCCEEDED(webview->add_DocumentTitleChanged(titleChanged, &s_state.documentTitleToken))
                        && SUCCEEDED(webview->add_NewWindowRequested(newWindow, &s_state.newWindowToken))) {
                        s_state.tokensRegistered = true;
                    }
                    navigationStarting->Release();
                    navigationCompleted->Release();
                    titleChanged->Release();
                    newWindow->Release();

                    auto* webMessage = new WebMessageReceivedHandler();
                    if (SUCCEEDED(webview->add_WebMessageReceived(webMessage, &s_state.webMessageToken))) {
                        s_state.webMessageRegistered = true;
                    }
                    webMessage->Release();

                    // 页面脚本注入要等控制器就绪，之前排队的脚本在这里补上
                    for (const std::string& script : s_state.pendingScripts) {
                        webview->AddScriptToExecuteOnDocumentCreated(WideFrom(script).c_str(), nullptr);
                    }
                    s_state.pendingScripts.clear();

                    s_state.canGoBack = false;
                    s_state.canGoForward = false;
                    s_state.url = SourceOf(webview);
                    s_state.title = TitleOf(webview);

                    if (!s_state.pendingHtml.empty()) {
                        webview->NavigateToString(WideFrom(s_state.pendingHtml).c_str());
                        s_state.pendingHtml.clear();
                    } else if (!s_state.pendingUrl.empty()) {
                        webview->Navigate(WideFrom(s_state.pendingUrl).c_str());
                        s_state.pendingUrl.clear();
                    }
                }
                s_state.initialized = true;
                if (s_state.visible) {
                    ApplyVisibleLocked();
                }
                ready = true;
            }
        }

        if (ready) {
            XBase::Log::Info("WebView: WebView2 控制器就绪");
        }
        NotifyStateChanged();
        return S_OK;
    }
};

class EnvironmentCompletedHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    XBASE_WEBVIEW_HANDLER_BODY(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* environment) override {
        if (FAILED(errorCode) || !environment) {
            {
                std::lock_guard<std::mutex> lock(s_state.mutex);
                s_state.createInFlight = false;
                s_state.createRequested = false;
            }
            XBase::Log::Error("WebView: WebView2 环境创建失败");
            NotifyStateChanged();
            return S_OK;
        }

        ICoreWebView2Environment* storedEnvironment = nullptr;
        HWND hostWindow = nullptr;
        bool cancelled = false;
        {
            std::lock_guard<std::mutex> lock(s_state.mutex);
            cancelled = s_state.shutdownPending || !s_state.createRequested;
            if (!cancelled) {
                s_state.environment = environment;
                s_state.environment->AddRef();
                storedEnvironment = s_state.environment;
                hostWindow = s_state.hostWindow;
            }
        }
        if (cancelled) {
            return S_OK;
        }
        if (!hostWindow || !IsWindow(hostWindow)) {
            XBase::Log::Error("WebView: 宿主窗口缺失，无法创建控制器");
            std::lock_guard<std::mutex> lock(s_state.mutex);
            s_state.createInFlight = false;
            ReleaseControllerLocked();
            return S_OK;
        }
        auto* handler = new ControllerCompletedHandler();
        const HRESULT hr = storedEnvironment->CreateCoreWebView2Controller(hostWindow, handler);
        handler->Release();
        if (FAILED(hr)) {
            XBase::Log::Error("WebView: 控制器创建请求失败");
        }
        return S_OK;
    }
};

bool EnsureHostWindow() {
    if (s_state.hostWindow) return true;
    if (!s_state.gameWindow) return false;

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &HostWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.lpszClassName = kHostWindowClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            XBase::Log::Error("WebView: 宿主窗口类注册失败");
            return false;
        }
        classRegistered = true;
    }

    s_state.hostWindow = CreateWindowExW(
        0, kHostWindowClass, L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 1, 1,
        s_state.gameWindow, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!s_state.hostWindow) {
        XBase::Log::Error("WebView: 宿主窗口创建失败");
        return false;
    }
    return true;
}

void DestroyHostWindow() {
    if (!s_state.hostWindow) return;
    DestroyWindow(s_state.hostWindow);
    s_state.hostWindow = nullptr;
}

LRESULT CALLBACK HostWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        XBase::Detail::Input::HandleVirtualKey(
            static_cast<std::uint32_t>(wParam), true, (lParam & (1LL << 30)) != 0);
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        XBase::Detail::Input::HandleVirtualKey(static_cast<std::uint32_t>(wParam), false, false);
        break;
    case WM_SIZE:
        if (s_state.controller) {
            RECT bounds{0, 0, LOWORD(lParam), HIWORD(lParam)};
            s_state.controller->put_Bounds(bounds);
        }
        return 0;
    case WM_CLOSE:
    case WM_DESTROY:
    case WM_SETFOCUS:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

namespace XBase::WebView {

bool IsRuntimeAvailable() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (s_state.runtimeState >= 0) {
        return s_state.runtimeState == 1;
    }
    if (!LoadRuntime()) {
        s_state.runtimeState = 0;
        return false;
    }
    LPWSTR version = nullptr;
    const HRESULT hr = s_runtime.getVersion(nullptr, &version);
    if (version) {
        CoTaskMemFree(version);
    }
    s_state.runtimeState = SUCCEEDED(hr) ? 1 : 0;
    return s_state.runtimeState == 1;
}

bool Init() {
    // 只登记请求。真正的创建延迟到宿主请求页面后的 Process 安全点执行，
    // 避免在渲染钩子里重入消息循环，也避免未使用网页时启动源码进程。
    if (!IsRuntimeAvailable()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.initRequested = true;
    return true;
}

bool IsInitialized() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    return s_state.initRequested || s_state.initialized;
}

void NotifyGameInit() {
    SetVisible(false);
}

void ProcessCreation();

// 菜单关闭后网页面板失去宿主，自动隐藏，避免覆盖层残留并持续抢占焦点
void ProcessMenuTransition() {
    const bool menuVisible = XBase::Hooks::IsMenuVisible();

    bool shouldHide = false;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        if (menuVisible) {
            s_state.menuWasVisible = true;
        } else if (s_state.menuWasVisible) {
            s_state.menuWasVisible = false;
            shouldHide = s_state.visible;
        }
    }
    if (shouldHide) {
        SetVisible(false);
    }
}

// 独占全屏下 HWND 覆盖层不可见，改为定时抓帧供宿主以贴图绘制
void ProcessCapture() {
    const unsigned long long now = static_cast<unsigned long long>(Platform::MonotonicMilliseconds());

    bool wantCapture = false;
    bool modeChanged = false;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        if (s_state.lastModeCheckAt == 0 || now - s_state.lastModeCheckAt >= kCaptureModeCheckMs) {
            s_state.lastModeCheckAt = now;
            const bool exclusive = IsExclusiveFullscreen();
            if (exclusive != s_state.captureMode) {
                s_state.captureMode = exclusive;
                modeChanged = true;
            }
        }
        if (s_state.captureMode && s_state.initialized && s_state.visible && !s_state.captureInFlight) {
            const unsigned long long interval =
                now - s_state.lastInteractionAt < kCaptureActiveWindowMs
                    ? kCaptureActiveIntervalMs
                    : kCaptureIdleIntervalMs;
            wantCapture = s_state.lastCaptureAt == 0 || now - s_state.lastCaptureAt >= interval;
        }
        if (modeChanged && s_state.hostWindow && s_state.gameWindow) {
            // 抓帧模式改变渲染表面大小，重新应用一次边界
            s_state.boundsApplied = false;
            ApplyBoundsLocked();
        }
    }
    if (wantCapture) {
        StartCapture();
    }
}

// 面板获得焦点期间按键只到达网页子窗口，用系统状态补齐输入，保证菜单热键仍可关闭菜单
void ProcessKeyboardFallback() {
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        if (!s_state.initialized || !s_state.visible) return;
    }
    XBase::Detail::Input::PollFromSystem();
}

// 游戏在游玩状态会隐藏系统光标，面板以原生窗口显示时需要把光标重新显示出来；
// 独占全屏抓帧预览不占屏幕，保持游戏自己的光标状态
void ProcessCursorVisibility() {
    bool panelShown = false;
    HWND hostWindow = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        panelShown = s_state.initialized && s_state.visible && !s_state.captureMode;
        hostWindow = s_state.hostWindow;
    }

    if (panelShown) {
        CURSORINFO info{};
        info.cbSize = sizeof(info);
        if (GetCursorInfo(&info) && info.flags == 0 && ShowCursor(TRUE) >= 0) {
            ++s_state.cursorShows;
        }

        // 游戏在游玩状态会把光标形状设成空，落在面板上时补回箭头形状，
        // 否则只有点击让网页子窗口拿到焦点后光标才可见
        if (hostWindow && IsWindow(hostWindow)) {
            POINT cursor{};
            RECT client{};
            if (GetCursorPos(&cursor) && GetClientRect(hostWindow, &client)) {
                POINT topLeft{client.left, client.top};
                POINT bottomRight{client.right, client.bottom};
                if (ClientToScreen(hostWindow, &topLeft) && ClientToScreen(hostWindow, &bottomRight)) {
                    const RECT screenRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
                    if (PtInRect(&screenRect, cursor)) {
                        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
                    }
                }
            }
        }
        return;
    }

    if (s_state.cursorShows > 0) {
        for (int index = 0; index < s_state.cursorShows; ++index) {
            ShowCursor(FALSE);
        }
        s_state.cursorShows = 0;
    }
}

void Process() {
    ProcessMenuTransition();
    ProcessCreation();
    ProcessCapture();
    ProcessKeyboardFallback();
    ProcessCursorVisibility();
}

void ProcessCreation() {
    if (!s_state.createRequested || s_state.initialized || s_state.createInFlight
        || s_state.shutdownPending) {
        return;
    }

    const HWND gameWindow = Detail::Hooks::GetGameWindow();
    if (!gameWindow) {
        return;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        Log::Error("WebView: COM 初始化失败");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        if (!s_state.createRequested || s_state.initialized || s_state.createInFlight) {
            return;
        }
        s_state.gameWindow = gameWindow;
        if (!EnsureHostWindow()) {
            s_state.createRequested = false;
            Log::Error("WebView: 宿主窗口创建失败");
            return;
        }
    }

    const std::string dataFolder = ModuleFilePath("XBase\\webview2");
    Platform::EnsureDirectory(dataFolder);

    auto* handler = new EnvironmentCompletedHandler();
    const HRESULT hr = s_runtime.createEnvironment(
        nullptr,
        WideFrom(dataFolder).c_str(),
        nullptr,
        handler);
    handler->Release();

    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (FAILED(hr)) {
        s_state.createRequested = false;
        Log::Error("WebView: WebView2 环境创建请求失败");
        return;
    }
    s_state.createInFlight = true;
}

// 关闭面板会释放浏览器与宿主窗口，之后再次 SetVisible(true) 会重新创建
bool Close() {
    if (!IsRuntimeAvailable()) return false;

    SetVisible(false);

    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.initRequested = true;
    s_state.createRequested = false;
    if (s_state.createInFlight) {
        s_state.shutdownPending = true;
        return true;
    }
    ReleaseControllerLocked();
    return true;
}

void Shutdown() {
    SetVisible(false);

    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.initRequested = false;
    s_state.createRequested = false;
    if (s_state.createInFlight) {
        // 创建中的控制器到达后自行释放并销毁窗口，避免父窗口句柄被复用
        s_state.shutdownPending = true;
        return;
    }
    ReleaseControllerLocked();
}

bool Navigate(const std::string& url) {
    if (url.empty()) return false;
    if (!IsRuntimeAvailable()) return false;
    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.initRequested = true;
    s_state.createRequested = true;
    if (!s_state.webview) {
        s_state.pendingUrl = url;
        s_state.pendingHtml.clear();
        return true;
    }
    return SUCCEEDED(s_state.webview->Navigate(WideFrom(url).c_str()));
}

bool SetHtml(const std::string& html) {
    if (html.empty()) return false;
    if (!IsRuntimeAvailable()) return false;
    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.initRequested = true;
    s_state.createRequested = true;
    if (!s_state.webview) {
        s_state.pendingHtml = html;
        s_state.pendingUrl.clear();
        return true;
    }
    return SUCCEEDED(s_state.webview->NavigateToString(WideFrom(html).c_str()));
}

void Reload() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (s_state.webview) {
        s_state.webview->Reload();
    }
}

bool GoBack() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (!s_state.webview || !s_state.canGoBack) return false;
    return SUCCEEDED(s_state.webview->GoBack());
}

bool GoForward() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (!s_state.webview || !s_state.canGoForward) return false;
    return SUCCEEDED(s_state.webview->GoForward());
}

void SetZoom(float factor) {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (factor < 0.25f) factor = 0.25f;
    if (factor > 4.0f) factor = 4.0f;
    s_state.zoom = factor;
    ApplyZoomLocked();
}

void SetVisible(bool visible) {
    // 显示面板即视为请求懒创建，创建在 Process 的安全点执行
    const bool runtimeAvailable = visible ? IsRuntimeAvailable() : false;
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (s_state.visible == visible) return;
    s_state.visible = visible;
    if (visible) {
        if (!runtimeAvailable) {
            s_state.visible = false;
            return;
        }
        s_state.initRequested = true;
        s_state.createRequested = true;
        if (!s_state.boundsApplied) {
            ApplyBoundsLocked();
        }
    }
    ApplyVisibleLocked();
}

bool IsVisible() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    return s_state.visible;
}

void SetBounds(const Rect& bounds) {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (s_state.boundsApplied
        && s_state.bounds.left == bounds.left
        && s_state.bounds.top == bounds.top
        && s_state.bounds.right == bounds.right
        && s_state.bounds.bottom == bounds.bottom) {
        return;
    }
    s_state.bounds = bounds;
    ApplyBoundsLocked();
}

State GetState() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    State state;
    state.initialized = s_state.initialized;
    state.visible = s_state.visible;
    state.loading = s_state.loading;
    state.canGoBack = s_state.canGoBack;
    state.canGoForward = s_state.canGoForward;
    state.lastError = s_state.lastError;
    state.url = s_state.url;
    state.title = s_state.title;
    return state;
}

void SetStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.stateCallback = callback;
}

void SetMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    s_state.messageHandler = std::move(handler);
}

bool PostJson(const std::string& json) {
    if (json.empty()) return false;
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (!s_state.webview) return false;
    return SUCCEEDED(s_state.webview->PostWebMessageAsJson(WideFrom(json).c_str()));
}

bool InjectScript(const std::string& script) {
    if (script.empty()) return false;
    std::lock_guard<std::mutex> lock(s_state.mutex);
    if (!s_state.webview) {
        s_state.pendingScripts.push_back(script);
        return true;
    }
    return SUCCEEDED(s_state.webview->AddScriptToExecuteOnDocumentCreated(WideFrom(script).c_str(), nullptr));
}

bool UsesCaptureMode() {
    std::lock_guard<std::mutex> lock(s_state.mutex);
    return s_state.initialized && s_state.captureMode;
}

void DrawPanel(const Rect& bounds) {
    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    if (width <= 0.0f || height <= 0.0f) return;

    std::lock_guard<std::mutex> lock(s_state.mutex);
    ImGui::SetCursorScreenPos(ImVec2(bounds.left, bounds.top));
    if (s_state.texture && s_state.previewReady) {
        ImGui::Image(reinterpret_cast<ImTextureID>(s_state.texture), ImVec2(width, height));
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 minimum(bounds.left, bounds.top);
    const ImVec2 maximum(bounds.left + width, bounds.top + height);
    drawList->AddRectFilled(minimum, maximum, IM_COL32(18, 20, 26, 235));
    drawList->AddText(ImVec2(minimum.x + 14.0f, minimum.y + 14.0f), IM_COL32(190, 196, 210, 255), "...");
    ImGui::Dummy(ImVec2(width, height));
}

void ForwardPanelInput(const Rect& bounds, Vec2 mouse, bool mouseDown, float wheelDelta) {
    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    if (width <= 0.0f || height <= 0.0f) return;

    int pageWidth = 0;
    int pageHeight = 0;
    {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        pageWidth = s_state.textureWidth;
        pageHeight = s_state.textureHeight;
    }
    if (pageWidth <= 0 || pageHeight <= 0) return;

    const float x = std::clamp((mouse.x - bounds.left) * pageWidth / width, 0.0f, pageWidth - 1.0f);
    const float y = std::clamp((mouse.y - bounds.top) * pageHeight / height, 0.0f, pageHeight - 1.0f);

    const bool clickEdge = mouseDown && !s_state.previousMouseDown;
    s_state.previousMouseDown = mouseDown;

    const unsigned long long now = static_cast<unsigned long long>(Platform::MonotonicMilliseconds());
    const bool movedEnough = !s_state.forwardedMoveValid
        || std::abs(x - s_state.lastForwardedMoveX) >= kInteractionMoveThreshold
        || std::abs(y - s_state.lastForwardedMoveY) >= kInteractionMoveThreshold;
    const bool moved = movedEnough && now - s_state.lastForwardedMoveAt >= kMoveForwardIntervalMs;
    if (moved) {
        s_state.lastForwardedMoveX = x;
        s_state.lastForwardedMoveY = y;
        s_state.lastForwardedMoveAt = now;
        s_state.forwardedMoveValid = true;
        char script[512]{};
        std::snprintf(script, sizeof(script),
            "(function(){var e=document.elementFromPoint(%.1f,%.1f);if(!e)return;"
            "e.dispatchEvent(new MouseEvent('mousemove',{bubbles:true,cancelable:true,clientX:%.1f,clientY:%.1f,view:window}));})()",
            x, y, x, y);
        ExecuteScript(script);
    }

    if (clickEdge) {
        char script[512]{};
        std::snprintf(script, sizeof(script),
            "(function(){var e=document.elementFromPoint(%.1f,%.1f);if(!e)return;"
            "var o={bubbles:true,cancelable:true,clientX:%.1f,clientY:%.1f,view:window};"
            "e.focus&&e.focus();"
            "e.dispatchEvent(new MouseEvent('mousedown',o));"
            "e.dispatchEvent(new MouseEvent('mouseup',o));"
            "e.dispatchEvent(new MouseEvent('click',o));})()",
            x, y, x, y);
        ExecuteScript(script);
    }
    if (wheelDelta != 0.0f) {
        char script[128]{};
        std::snprintf(script, sizeof(script), "window.scrollBy(0,%.0f);", static_cast<double>(-wheelDelta * 120.0f));
        ExecuteScript(script);
    }

    if (moved || clickEdge || wheelDelta != 0.0f) {
        std::lock_guard<std::mutex> lock(s_state.mutex);
        s_state.lastInteractionAt = now;
        if (clickEdge || wheelDelta != 0.0f) {
            s_state.lastCaptureAt = 0;
        }
    }
}

} // namespace XBase::WebView
