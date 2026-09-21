#include <windows.h>

extern "C" void XBasePayloadAttach();
extern "C" void XBasePayloadDetach();

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        XBasePayloadAttach();
    } else if (reason == DLL_PROCESS_DETACH) {
        XBasePayloadDetach();
    }
    return TRUE;
}