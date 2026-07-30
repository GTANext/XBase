#pragma once

#include <functional>
#include <d3d9.h>

namespace XBase::Hooks {

bool Init();
void Shutdown();
bool IsInitialized();
bool IsReady();

void SetDrawCallback(std::function<void()> callback);
void SetMenuVisible(bool visible);
bool IsMenuVisible();
void ToggleMenu();

LPDIRECT3DDEVICE9 GetDevice();

} // namespace XBase::Hooks
