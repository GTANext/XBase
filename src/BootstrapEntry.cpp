#include "Bootstrap.h"

#include <cstdint>
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        XBase::Bootstrap::Attach(reinterpret_cast<std::uintptr_t>(module));
    } else if (reason == DLL_PROCESS_DETACH) {
        XBase::Bootstrap::Detach();
    }
    return TRUE;
}