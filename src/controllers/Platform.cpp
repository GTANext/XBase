#include <XBase/Platform.h>

#include <chrono>
#include <cstring>
#include <windows.h>
#include <shellapi.h>

namespace XBase::Platform {

bool ShowError(const char* title, const char* message) {
    if (!message || !message[0]) return false;
    MessageBoxA(
        HWND_DESKTOP,
        message,
        title && title[0] ? title : "XBase",
        MB_OK | MB_ICONERROR);
    return true;
}

bool OpenExternal(const char* target) {
    if (!target || !target[0]) return false;
    return reinterpret_cast<std::intptr_t>(
        ShellExecuteA(nullptr, "open", target, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool SetClipboardText(const char* text) {
    if (!text || !OpenClipboard(nullptr)) return false;

    bool copied = false;
    if (EmptyClipboard()) {
        const std::size_t size = std::strlen(text) + 1;
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (memory) {
            void* target = GlobalLock(memory);
            if (target) {
                std::memcpy(target, text, size);
                GlobalUnlock(memory);
                if (SetClipboardData(CF_TEXT, memory)) {
                    copied = true;
                    memory = nullptr;
                }
            }
            if (memory) GlobalFree(memory);
        }
    }

    CloseClipboard();
    return copied;
}

std::uint64_t MonotonicMilliseconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

namespace {

std::string DirectoryFromModule(HMODULE module) {
    if (!module) return {};

    std::string path(MAX_PATH, '\0');
    for (;;) {
        const DWORD size = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0) return {};
        if (size < path.size() - 1) {
            path.resize(size);
            break;
        }
        path.resize(path.size() * 2);
    }

    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
}

} // namespace

std::string ModuleDirectory(const char* moduleName) {
    if (!moduleName || !moduleName[0]) return {};
    return DirectoryFromModule(GetModuleHandleA(moduleName));
}

std::string CurrentModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&CurrentModuleDirectory),
            &module)) {
        return {};
    }
    return DirectoryFromModule(module);
}

bool EnsureDirectory(const std::string& path) {
    if (path.empty()) return false;
    if (CreateDirectoryA(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool FileExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

} // namespace XBase::Platform