#pragma once

#include <cstdint>

namespace XBase::Detail::Input {

void HandleVirtualKey(std::uint32_t virtualKey, bool down, bool repeat);
void Reset();

} // namespace XBase::Detail::Input