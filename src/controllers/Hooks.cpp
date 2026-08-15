#include <XBase/Hooks.h>
#include <XBase/Log.h>
#include <XBase/Theme.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

#if defined(XBASE_WITH_KIERO)
#include <Windows.h>
#include <d3d9.h>
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx9.h"
#include "plugin.h"
#include "CPad.h"
#include "InputInternal.h"
#include "RenderFonts.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

using EndSceneFn = HRESULT(__stdcall*)(LPDIRECT3DDEVICE9);
using ResetFn = HRESULT(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

struct DrawCallbackEntry {
    XBase::Hooks::DrawCallbackId id;
    std::function<void()> callback;
};

XBase::Hooks::RuntimeState g_state = XBase::Hooks::RuntimeState::Uninitialized;
std::mutex g_drawCallbacksMutex;
std::vector<DrawCallbackEntry> g_drawCallbacks;
std::uint64_t g_nextDrawCallbackId = 1;
bool g_menuVisible = false;
bool g_backgroundInputActive = false;
bool g_backgroundRenderActive = false;
float g_wheelDelta = 0.0f;
const char* g_statusText = "not initialized";
HWND g_window = nullptr;
WNDPROC g_originalWndProc = nullptr;
EndSceneFn g_originalEndScene = nullptr;
ResetFn g_originalReset = nullptr;
bool g_gameInputBlocked = false;
std::atomic<bool> g_shutdownRequested{false};
std::atomic<unsigned int> g_activeRenderCallbacks{0};
std::mutex g_renderCallbacksMutex;
std::condition_variable g_renderCallbacksIdle;

class RenderCallbackScope {
public:
    RenderCallbackScope() {
        g_activeRenderCallbacks.fetch_add(1, std::memory_order_acq_rel);
        renderAllowed_ = !g_shutdownRequested.load(std::memory_order_acquire);
    }

    ~RenderCallbackScope() {
        if (g_activeRenderCallbacks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            g_renderCallbacksIdle.notify_all();
        }
    }

    explicit operator bool() const { return renderAllowed_; }

private:
    bool renderAllowed_ = false;
};

void WaitForRenderCallbacks() {
    std::unique_lock<std::mutex> lock(g_renderCallbacksMutex);
    g_renderCallbacksIdle.wait(lock, []() {
        return g_activeRenderCallbacks.load(std::memory_order_acquire) == 0;
    });
}

std::vector<std::function<void()>> g_drawCallbacksSnapshot;
bool g_drawCallbacksSnapshotDirty = true;

const std::vector<std::function<void()>>& SnapshotDrawCallbacks() {
    std::lock_guard<std::mutex> lock(g_drawCallbacksMutex);
    if (!g_drawCallbacksSnapshotDirty) {
        return g_drawCallbacksSnapshot;
    }
    g_drawCallbacksSnapshot.clear();
    g_drawCallbacksSnapshot.reserve(g_drawCallbacks.size());
    for (const DrawCallbackEntry& entry : g_drawCallbacks) {
        g_drawCallbacksSnapshot.push_back(entry.callback);
    }
    g_drawCallbacksSnapshotDirty = false;
    return g_drawCallbacksSnapshot;
}

struct InputPatchSnapshot {
    uintptr_t address = 0;
    std::array<unsigned char, 5> bytes{};
    std::size_t size = 0;
    bool captured = false;
};

std::array<InputPatchSnapshot, 4> g_inputPatches;

void CaptureInputPatch(InputPatchSnapshot& patch) {
    if (patch.captured || patch.address == 0 || patch.size == 0) return;
    plugin::patch::GetRaw(patch.address, patch.bytes.data(), patch.size);
    patch.captured = true;
}

void RestoreInputPatch(InputPatchSnapshot& patch) {
    if (!patch.captured) return;
    plugin::patch::SetRaw(patch.address, patch.bytes.data(), patch.size);
    patch.captured = false;
}

void ConfigureInputPatches() {
#if defined(GTASA)
    g_inputPatches = {{{0x6194A0, {}, 1, false}, {0x541DD7, {}, 5, false}, {0x4EB731, {}, 1, false}, {0x4EB75A, {}, 1, false}}};
#elif defined(GTAVC)
    g_inputPatches = {{{0x6020A0, {}, 1, false}, {0x4AB6CA, {}, 5, false}, {}, {}}};
#elif defined(GTA3)
    g_inputPatches = {{{0x580D20, {}, 1, false}, {0x49272F, {}, 5, false}, {}, {}}};
#else
    g_inputPatches = {};
#endif
}

void ClearMouseState() {
    CPad* pad = CPad::GetPad(0);
    if (!pad) return;
    CPad::NewMouseControllerState = {};
    CPad::OldMouseControllerState = {};
    CPad::PCTempMouseControllerState = {};
    (void)pad;
}

void ApplyGameInputBlock(bool blocked) {
    if (g_gameInputBlocked != blocked) {
        g_gameInputBlocked = blocked;
        ClearMouseState();
        if (blocked) {
            for (InputPatchSnapshot& patch : g_inputPatches) CaptureInputPatch(patch);
#if defined(GTASA)
            plugin::patch::SetUChar(0x6194A0, 0xC3);
            plugin::patch::Nop(0x541DD7, 5);
            plugin::patch::SetUChar(0x4EB731, 0xEB);
            plugin::patch::SetUChar(0x4EB75A, 0xEB);
#elif defined(GTAVC)
            plugin::patch::SetUChar(0x6020A0, 0xC3);
            plugin::patch::Nop(0x4AB6CA, 5);
#elif defined(GTA3)
            plugin::patch::SetUChar(0x580D20, 0xC3);
            plugin::patch::Nop(0x49272F, 5);
#endif
        } else {
            for (InputPatchSnapshot& patch : g_inputPatches) RestoreInputPatch(patch);
            // 菜单期间 DirectInput 鼠标增量被拦截后累积在缓冲里，
            // 恢复补丁后先跑一次 UpdatePads 把积压增量读入 CPad 状态，再清零丢弃，
            // 否则下一帧相机会按菜单期间的鼠标移动量转动。
            CPad::UpdatePads();
            ClearMouseState();
        }
    }

    CPad* pad = CPad::GetPad(0);
    if (pad) pad->DisablePlayerControls = blocked;
}

bool WantsInput() {
    return g_menuVisible || g_backgroundInputActive;
}

bool WantsRender() {
    return g_menuVisible || g_backgroundInputActive || g_backgroundRenderActive;
}

void ReleaseCursor() {
    if (ImGui::GetCurrentContext()) {
        ImGui::GetIO().MouseDrawCursor = false;
    }
}

void SyncCursorPosition() {
    if (!g_window || !ImGui::GetCurrentContext()) return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 1.0f || io.DisplaySize.y <= 1.0f) return;

    POINT cursor{};
    RECT client{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(g_window, &cursor) || !GetClientRect(g_window, &client)) return;

    const float width = static_cast<float>(client.right - client.left);
    const float height = static_cast<float>(client.bottom - client.top);
    if (width <= 1.0f || height <= 1.0f) return;

    float x = static_cast<float>(cursor.x) * io.DisplaySize.x / width;
    float y = static_cast<float>(cursor.y) * io.DisplaySize.y / height;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x >= io.DisplaySize.x) x = io.DisplaySize.x - 1.0f;
    if (y >= io.DisplaySize.y) y = io.DisplaySize.y - 1.0f;
    io.AddMousePosEvent(x, y);
}

bool IsMouseMessage(UINT message) {
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_INPUT:
        return true;
    default:
        return false;
    }
}

bool IsKeyboardMessage(UINT message) {
    switch (message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
        return true;
    default:
        return false;
    }
}

LRESULT __stdcall WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    RenderCallbackScope callbackScope;
    if (!callbackScope) {
        return g_originalWndProc
            ? CallWindowProc(g_originalWndProc, window, message, wParam, lParam)
            : DefWindowProc(window, message, wParam, lParam);
    }

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        const bool repeat = (lParam & (1LL << 30)) != 0;
        XBase::Detail::Input::HandleVirtualKey(static_cast<std::uint32_t>(wParam), true, repeat);
    } else if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        XBase::Detail::Input::HandleVirtualKey(static_cast<std::uint32_t>(wParam), false, false);
    }

    if (WantsInput() && ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
        const ImGuiIO& io = ImGui::GetIO();
        if ((IsMouseMessage(message) && io.WantCaptureMouse)
            || (IsKeyboardMessage(message) && (io.WantCaptureKeyboard || io.WantTextInput))) {
            return 1;
        }
    } else if (message == WM_MOUSEWHEEL) {
        g_wheelDelta += static_cast<float>(static_cast<short>(HIWORD(wParam))) / static_cast<float>(WHEEL_DELTA);
#if defined(GTASA)
    } else if (message == WM_INPUT) {
        RAWINPUT input{};
        UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) == sizeof(input)
            && input.header.dwType == RIM_TYPEMOUSE
            && (input.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)) {
            g_wheelDelta += static_cast<float>(static_cast<short>(input.data.mouse.usButtonData)) / static_cast<float>(WHEEL_DELTA);
        }
#endif
    }

    return g_originalWndProc
        ? CallWindowProc(g_originalWndProc, window, message, wParam, lParam)
        : DefWindowProc(window, message, wParam, lParam);
}

bool InitRenderRuntime(LPDIRECT3DDEVICE9 device) {
    D3DDEVICE_CREATION_PARAMETERS parameters{};
    if (FAILED(device->GetCreationParameters(&parameters)) || !parameters.hFocusWindow || !IsWindow(parameters.hFocusWindow)) {
        g_state = XBase::Hooks::RuntimeState::Failed;
        g_statusText = "D3D9 hook failed: invalid game window";
        XBase::Log::Error("Hooks: invalid D3D9 focus window");
        return false;
    }

    g_window = parameters.hFocusWindow;
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!g_originalWndProc) {
        g_state = XBase::Hooks::RuntimeState::Failed;
        g_statusText = "D3D9 hook failed: WndProc installation failed";
        XBase::Log::Error("Hooks: WndProc installation failed");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    const bool win32Initialized = ImGui_ImplWin32_Init(g_window);
    const bool dx9Initialized = win32Initialized && ImGui_ImplDX9_Init(device);
    if (!win32Initialized || !dx9Initialized) {
        if (dx9Initialized) ImGui_ImplDX9_Shutdown();
        if (win32Initialized) ImGui_ImplWin32_Shutdown();
        SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
        g_window = nullptr;
        ImGui::DestroyContext();
        g_state = XBase::Hooks::RuntimeState::Failed;
        g_statusText = "D3D9 hook failed: ImGui backend initialization failed";
        XBase::Log::Error("Hooks: ImGui backend initialization failed");
        return false;
    }

    ImGui::StyleColorsDark();
    XBase::Theme::Init();
    XBase::Detail::RenderFonts::LoadDefault();
    g_state = XBase::Hooks::RuntimeState::RenderReady;
    g_statusText = "D3D9 render ready";
    XBase::Log::Info("Hooks: D3D9 and ImGui render runtime ready");
    return true;
}

HRESULT __stdcall EndScene(LPDIRECT3DDEVICE9 device) {
    RenderCallbackScope callbackScope;
    if (!callbackScope) {
        return g_originalEndScene ? g_originalEndScene(device) : S_OK;
    }
    if (g_state == XBase::Hooks::RuntimeState::Hooked && !InitRenderRuntime(device)) {
        return g_originalEndScene ? g_originalEndScene(device) : S_OK;
    }

    if (g_state == XBase::Hooks::RuntimeState::RenderReady && WantsRender()) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (WantsInput()) SyncCursorPosition();
        ImGui::NewFrame();
        ImGui::GetIO().MouseDrawCursor = WantsInput();

        for (const auto& drawCallback : SnapshotDrawCallbacks()) {
            if (drawCallback) drawCallback();
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return g_originalEndScene ? g_originalEndScene(device) : S_OK;
}

HRESULT __stdcall Reset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* parameters) {
    RenderCallbackScope callbackScope;
    if (!callbackScope) {
        return g_originalReset ? g_originalReset(device, parameters) : D3DERR_INVALIDCALL;
    }
    if (!g_originalReset) return D3DERR_INVALIDCALL;
    if (g_state != XBase::Hooks::RuntimeState::RenderReady) {
        return g_originalReset(device, parameters);
    }

    ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT result = g_originalReset(device, parameters);
    if (SUCCEEDED(result)) ImGui_ImplDX9_CreateDeviceObjects();
    return result;
}

} // namespace
#endif

namespace XBase::Hooks {

bool Init() {
#if defined(XBASE_WITH_KIERO)
    if (g_state == RuntimeState::Hooked || g_state == RuntimeState::RenderReady) return true;
    if (g_state == RuntimeState::ShuttingDown) return false;

    g_shutdownRequested.store(false, std::memory_order_release);
    XBase::Detail::Input::Reset();
    ConfigureInputPatches();
    g_state = RuntimeState::Uninitialized;
    g_statusText = "initializing";
    if (kiero::init(kiero::RenderType::D3D9) != kiero::Status::Success) {
        g_state = RuntimeState::Failed;
        g_statusText = "D3D9 hook initialization failed";
        Log::Error("Hooks: kiero initialization failed");
        return false;
    }

    if (kiero::bind(42, reinterpret_cast<void**>(&g_originalEndScene), reinterpret_cast<void*>(EndScene)) != kiero::Status::Success) {
        kiero::shutdown();
        g_state = RuntimeState::Failed;
        g_statusText = "D3D9 hook failed: EndScene binding failed";
        Log::Error("Hooks: EndScene binding failed");
        return false;
    }

    if (kiero::bind(16, reinterpret_cast<void**>(&g_originalReset), reinterpret_cast<void*>(Reset)) != kiero::Status::Success) {
        kiero::shutdown();
        g_originalEndScene = nullptr;
        g_state = RuntimeState::Failed;
        g_statusText = "D3D9 hook failed: Reset binding failed";
        Log::Error("Hooks: Reset binding failed");
        return false;
    }

    g_state = RuntimeState::Hooked;
    g_statusText = "D3D9 hook installed";
    Log::Info("Hooks: D3D9 hooks installed");
    return true;
#else
    return false;
#endif
}

void Shutdown() {
#if defined(XBASE_WITH_KIERO)
    if (g_state == RuntimeState::Uninitialized || g_state == RuntimeState::ShuttingDown) return;

    const RuntimeState previousState = g_state;
    g_state = RuntimeState::ShuttingDown;
    g_shutdownRequested.store(true, std::memory_order_release);
    g_menuVisible = false;
    g_backgroundInputActive = false;
    g_backgroundRenderActive = false;
    ApplyGameInputBlock(false);

    if (g_window && g_originalWndProc) {
        SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
    }
    kiero::shutdown();
    WaitForRenderCallbacks();

    if (previousState == RuntimeState::RenderReady) {
        ReleaseCursor();
        XBase::Theme::Shutdown();
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    {
        std::lock_guard<std::mutex> lock(g_drawCallbacksMutex);
        g_drawCallbacks.clear();
        g_drawCallbacksSnapshotDirty = true;
    }
    g_originalEndScene = nullptr;
    g_originalReset = nullptr;
    g_originalWndProc = nullptr;
    g_window = nullptr;
    g_wheelDelta = 0.0f;
    XBase::Detail::Input::Reset();
    g_statusText = "not initialized";
    g_state = RuntimeState::Uninitialized;
#endif
}

RuntimeState GetState() {
#if defined(XBASE_WITH_KIERO)
    return g_state;
#else
    return RuntimeState::Uninitialized;
#endif
}

bool IsInitialized() {
    const RuntimeState state = GetState();
    return state == RuntimeState::Hooked || state == RuntimeState::RenderReady;
}

bool IsReady() {
    return GetState() == RuntimeState::RenderReady;
}

bool HadInitFailure() {
    return GetState() == RuntimeState::Failed;
}

const char* GetStatusText() {
#if defined(XBASE_WITH_KIERO)
    return g_statusText;
#else
    return "hooks unavailable";
#endif
}

DrawCallbackId RegisterDrawCallback(std::function<void()> callback) {
#if defined(XBASE_WITH_KIERO)
    if (!callback) return {};

    std::lock_guard<std::mutex> lock(g_drawCallbacksMutex);
    const DrawCallbackId callbackId{g_nextDrawCallbackId++};
    g_drawCallbacks.push_back({callbackId, std::move(callback)});
    g_drawCallbacksSnapshotDirty = true;
    return callbackId;
#else
    (void)callback;
    return {};
#endif
}

bool UnregisterDrawCallback(DrawCallbackId callbackId) {
#if defined(XBASE_WITH_KIERO)
    if (!callbackId) return false;

    std::lock_guard<std::mutex> lock(g_drawCallbacksMutex);
    const auto callback = std::find_if(
        g_drawCallbacks.begin(),
        g_drawCallbacks.end(),
        [callbackId](const DrawCallbackEntry& entry) {
            return entry.id.value == callbackId.value;
        });
    if (callback == g_drawCallbacks.end()) return false;
    g_drawCallbacks.erase(callback);
    g_drawCallbacksSnapshotDirty = true;
    return true;
#else
    (void)callbackId;
    return false;
#endif
}

void SetMenuVisible(bool visible) {
#if defined(XBASE_WITH_KIERO)
    if (!IsInitialized()) visible = false;
    g_menuVisible = visible;
    ApplyGameInputBlock(WantsInput());
    if (!WantsInput()) ReleaseCursor();
#else
    (void)visible;
#endif
}

bool IsMenuVisible() {
#if defined(XBASE_WITH_KIERO)
    return g_menuVisible;
#else
    return false;
#endif
}

void ToggleMenu() {
    SetMenuVisible(!IsMenuVisible());
}

void SetBackgroundInputActive(bool active) {
#if defined(XBASE_WITH_KIERO)
    g_backgroundInputActive = active;
    ApplyGameInputBlock(WantsInput());
    if (!WantsInput()) ReleaseCursor();
#else
    (void)active;
#endif
}

bool IsBackgroundInputActive() {
#if defined(XBASE_WITH_KIERO)
    return g_backgroundInputActive;
#else
    return false;
#endif
}

void SetBackgroundRenderActive(bool active) {
#if defined(XBASE_WITH_KIERO)
    g_backgroundRenderActive = active;
#else
    (void)active;
#endif
}

bool IsBackgroundRenderActive() {
#if defined(XBASE_WITH_KIERO)
    return g_backgroundRenderActive;
#else
    return false;
#endif
}

void MaintainInputState() {
#if defined(XBASE_WITH_KIERO)
    ApplyGameInputBlock(WantsInput());
    if (g_state != RuntimeState::RenderReady || !ImGui::GetCurrentContext()) return;
    ImGui::GetIO().MouseDrawCursor = WantsInput();
#endif
}

float GetFrameDeltaSeconds() {
#if defined(XBASE_WITH_KIERO)
    if (g_state == RuntimeState::RenderReady && ImGui::GetCurrentContext()) {
        return ImGui::GetIO().DeltaTime;
    }
#endif
    return 1.0f / 60.0f;
}

bool IsKeyboardCaptureActive() {
#if defined(XBASE_WITH_KIERO)
    if (g_state == RuntimeState::RenderReady && ImGui::GetCurrentContext()) {
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput || io.WantCaptureKeyboard;
    }
#endif
    return false;
}

float ConsumeWheelDelta() {
#if defined(XBASE_WITH_KIERO)
    const float value = g_wheelDelta;
    g_wheelDelta = 0.0f;
    return value;
#else
    return 0.0f;
#endif
}

} // namespace XBase::Hooks