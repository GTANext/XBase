#include "Bootstrap.h"

#include <cstdint>
#include <windows.h>

// XBase 引导入口，只负责把共享运行时拉起来，不加载任何 mod
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        XBase::Bootstrap::AttachRuntime(reinterpret_cast<std::uintptr_t>(module));
    } else if (reason == DLL_PROCESS_DETACH) {
        XBase::Bootstrap::Detach();
    }
    return TRUE;
}
