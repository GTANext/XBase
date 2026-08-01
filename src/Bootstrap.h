#pragma once

#include <cstdint>

namespace XBase::Bootstrap {

using ModuleHandle = std::uintptr_t;

bool Attach(ModuleHandle loaderModule);
void Detach();
bool IsAttached();

} // namespace XBase::Bootstrap