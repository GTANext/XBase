#include <windows.h>

extern "C" void XMenuPayloadAttach();
extern "C" void XMenuPayloadDetach();

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        XMenuPayloadAttach();
    } else if (reason == DLL_PROCESS_DETACH) {
        XMenuPayloadDetach();
    }
    return TRUE;
}