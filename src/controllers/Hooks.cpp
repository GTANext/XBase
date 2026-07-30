#include <XBase/Hooks.h>
#include <XBase/Log.h>
#include <Windows.h>

#if defined(XBASE_WITH_KIERO)
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx9.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LPDIRECT3DDEVICE9 g_device = nullptr;
static bool g_initialized = false;
static bool g_ready = false;
static bool g_menuVisible = false;
static bool g_firstFrame = true;
static std::function<void()> g_drawCallback;
static WNDPROC g_origWndProc = nullptr;

static LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_menuVisible) {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        return 1;
    }
    return CallWindowProc(g_origWndProc, hWnd, msg, wParam, lParam);
}

static HRESULT __stdcall EndScene(LPDIRECT3DDEVICE9 device) {
    if (!g_ready) {
        g_device = device;
        g_device->AddRef();

        D3DDEVICE_CREATION_PARAMETERS cp;
        device->GetCreationParameters(&cp);
        HWND hwnd = cp.hFocusWindow;
        g_origWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX9_Init(device);
        ImGui::StyleColorsDark();

        g_ready = true;
        XBase::Log::Info("Hooks: D3D9 hooked, ImGui ready");
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_drawCallback) g_drawCallback();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return g_firstFrame ? S_OK : S_OK;
}

static HRESULT __stdcall Reset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* pp) {
    if (g_ready) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = S_OK;
        ImGui_ImplDX9_CreateDeviceObjects();
        return hr;
    }
    return S_OK;
}
#endif

namespace XBase::Hooks {

bool Init() {
#if defined(XBASE_WITH_KIERO)
    if (g_initialized) return true;

    if (kiero::init(kiero::RenderType::D3D9) != kiero::Status::Success) {
                    XBase::Log::Error("Hooks: kiero init failed");
                    return false;
                }

                if (kiero::bind(42, nullptr, reinterpret_cast<void**>(&EndScene)) != kiero::Status::Success) {
                    XBase::Log::Error("Hooks: kiero bind EndScene failed");
                    kiero::shutdown();
                    return false;
                }

                if (kiero::bind(16, nullptr, reinterpret_cast<void**>(&Reset)) != kiero::Status::Success) {
                    XBase::Log::Error("Hooks: kiero bind Reset failed");
                    kiero::shutdown();
                    return false;
                }

                g_initialized = true;
                XBase::Log::Info("Hooks: kiero initialized");
    return true;
#else
    XBase::Log::Warn("Hooks: kiero not available, stubs active");
    return false;
#endif
}

void Shutdown() {
#if defined(XBASE_WITH_KIERO)
    if (g_ready) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ready = false;
    }
    if (g_initialized) {
        kiero::shutdown();
        g_initialized = false;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
#endif
}

bool IsInitialized() {
    return g_initialized;
}

bool IsReady() {
    return g_ready;
}

void SetDrawCallback(std::function<void()> callback) {
    g_drawCallback = std::move(callback);
}

void SetMenuVisible(bool visible) {
    g_menuVisible = visible;
}

bool IsMenuVisible() {
    return g_menuVisible;
}

void ToggleMenu() {
    g_menuVisible = !g_menuVisible;
}

LPDIRECT3DDEVICE9 GetDevice() {
    return g_device;
}

} // namespace XBase::Hooks
