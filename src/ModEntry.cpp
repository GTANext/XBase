#include "Bootstrap.h"

#include <cstdint>
#include <windows.h>

extern "C" void XBasePayloadAttach();
extern "C" void XBasePayloadDetach();

// 单文件 asi 的入口：共享运行时就位后直接跑本模块里的业务入口
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (XBase::Bootstrap::Attach(reinterpret_cast<std::uintptr_t>(module))) {
            XBasePayloadAttach();
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        XBasePayloadDetach();
        XBase::Bootstrap::Detach();
    }
    return TRUE;
}
