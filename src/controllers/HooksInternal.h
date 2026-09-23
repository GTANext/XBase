#pragma once

#include <Windows.h>
#include <d3d9.h>

namespace XBase::Detail::Hooks {

// 游戏窗口句柄，仅在 Hooks 初始化完成后有效。
HWND GetGameWindow();

// 当前 ImGui 使用的 D3D9 设备，未就绪时返回 nullptr。
IDirect3DDevice9* GetD3D9Device();

// 当前 ImGui 显示尺寸，通常等于游戏后缓冲像素，未就绪时返回零
void GetDisplaySize(float& width, float& height);

} // namespace XBase::Detail::Hooks
