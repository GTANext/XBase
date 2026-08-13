#include <XBase/Platform.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

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

bool IsModuleLoaded(const char* moduleName) {
    return moduleName && moduleName[0] && GetModuleHandleA(moduleName) != nullptr;
}

bool EnsureDirectory(const std::string& path) {
    if (path.empty()) return false;
    if (CreateDirectoryA(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool DirectoryExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DownloadText(const char* url, std::string& output) {
    output.clear();
    if (!url || !url[0]) return false;

    IStream* stream = nullptr;
    if (FAILED(URLOpenBlockingStreamA(nullptr, url, &stream, 0, nullptr)) || !stream) {
        return false;
    }

    char buffer[4096];
    ULONG bytesRead = 0;
    bool success = true;
    do {
        const HRESULT result = stream->Read(buffer, sizeof(buffer), &bytesRead);
        if (FAILED(result)) {
            success = false;
            break;
        }
        output.append(buffer, bytesRead);
    } while (bytesRead != 0);

    stream->Release();
    if (!success || output.empty()) {
        output.clear();
        return false;
    }
    return true;
}

bool ReadTextFile(const std::string& path, std::string& output) {
    output.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return !file.fail();
}

std::vector<std::string> ListDirectories(const std::string& path) {
    std::vector<std::string> directories;
    if (path.empty()) return directories;

    std::string pattern = path;
    if (pattern.back() != '\\' && pattern.back() != '/') pattern += '\\';
    pattern += '*';

    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return directories;

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        const std::string name = data.cFileName;
        if (name != "." && name != "..") directories.push_back(name);
    } while (FindNextFileA(search, &data));
    FindClose(search);
    return directories;
}

bool ReadModuleResource(int resourceId, std::string& output) {
    output.clear();
    if (resourceId <= 0) return false;

    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ReadModuleResource),
            &module) || !module) {
        return false;
    }

    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) return false;
    const DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(loaded);
    if (!data || size == 0) return false;
    output.assign(static_cast<const char*>(data), size);
    return true;
}

} // namespace XBase::Platform