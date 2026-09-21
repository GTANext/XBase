#pragma once

#include <cstdint>

namespace XBase::Detail::Input {

void HandleVirtualKey(std::uint32_t virtualKey, bool down, bool repeat);
void Reset();

// 网页子窗口取得焦点后按键不再经过游戏窗口，用系统状态补齐按下与松开
void PollFromSystem();

} // namespace XBase::Detail::Input