#pragma once

#include <XBase/Abi.h>

#include <cstdint>

namespace XBase::Bootstrap {

using ModuleHandle = std::uintptr_t;

bool Attach(ModuleHandle loaderModule);

// 只把共享运行时拉起来，不找 payload。XBase.asi 用它，mod 的入口用 Attach
bool AttachRuntime(ModuleHandle loaderModule);
void Detach();
bool IsAttached();

// 共享运行时的函数表，适配层从这里取；运行时未就位时返回空指针
const XBaseRuntime* GetRuntimeTable();

} // namespace XBase::Bootstrap