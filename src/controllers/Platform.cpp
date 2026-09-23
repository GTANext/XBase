#include <XBase/Platform.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

namespace XBase::Platform {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), wide.data(), size);
    return wide;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

bool ShowError(const char* title, const char* message) {
    if (!message || !message[0]) return false;
    MessageBoxW(
        HWND_DESKTOP,
        Utf8ToWide(message).c_str(),
        Utf8ToWide(title && title[0] ? title : "XBase").c_str(),
        MB_OK | MB_ICONERROR);
    return true;
}

bool OpenExternal(const char* target) {
    if (!target || !target[0]) return false;
    return reinterpret_cast<std::intptr_t>(
        ShellExecuteW(nullptr, L"open", Utf8ToWide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool SetClipboardText(const char* text) {
    if (!text || !OpenClipboard(nullptr)) return false;

    const std::wstring wide = Utf8ToWide(text);
    bool copied = false;
    if (EmptyClipboard()) {
        const std::size_t size = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (memory) {
            void* target = GlobalLock(memory);
            if (target) {
                std::memcpy(target, wide.c_str(), size);
                GlobalUnlock(memory);
                if (SetClipboardData(CF_UNICODETEXT, memory)) {
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

    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD size = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0) return {};
        if (size < path.size() - 1) {
            path.resize(size);
            break;
        }
        path.resize(path.size() * 2);
    }

    const std::string utf8 = WideToUtf8(path);
    const std::size_t slash = utf8.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : utf8.substr(0, slash + 1);
}

} // namespace

std::string ModuleDirectory(const char* moduleName) {
    if (!moduleName || !moduleName[0]) return {};
    return DirectoryFromModule(GetModuleHandleW(Utf8ToWide(moduleName).c_str()));
}

std::string CurrentModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&CurrentModuleDirectory),
            &module)) {
        return {};
    }
    return DirectoryFromModule(module);
}

std::string GameDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    std::wstring path(buffer, length);
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    return WideToUtf8(path.substr(0, slash + 1));
}

std::string XBaseDirectory() {
    const std::string game = GameDirectory();
    if (game.empty()) {
        // 拿不到游戏目录时退回 asi 所在目录，至少保证可用
        return CurrentModuleDirectory() + "XBase\\";
    }
    return game + "XBase\\";
}

// 应用数据目录：<AppData>\com.yuinijika.xbase\
// 与游戏目录解耦：游戏目录常被整体压缩分享，缓存与 WebView2 用户数据落在那里会被一起打包带走。
// 只读环境变量而不调 SHGetKnownFolderPath，避免额外的 shell32 链接与 NTDDI 版本依赖。
std::string AppDataDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    std::wstring root(buffer, length);
    if (root.empty()) return {};
    if (root.back() != L'\\' && root.back() != L'/') root += L'\\';
    return WideToUtf8(root) + "com.yuinijika.xbase\\";
}

// 每个模组的数据放在 XBase 目录下以模组名命名的子目录，二进制统一放 Library 子目录，两者分开
// Mod 开头的函数与目录同名是正式命名，Runtime 开头的函数只是兼容别名，两者返回同一路径
std::string RuntimeDirectory(const char* modName) {
    const std::string name = (modName && modName[0]) ? modName : "Common";
    const std::string directory = XBaseDirectory() + "Mods\\" + name + "\\";
    EnsureDirectory(directory);
    return directory;
}

std::string RuntimeConfigPath(const char* modName) {
    return RuntimeDirectory(modName) + "config.json";
}

std::string RuntimeLogPath(const char* modName) {
    return RuntimeDirectory(modName) + "debug.log";
}

std::string ModDirectory(const char* modName) { return RuntimeDirectory(modName); }
std::string ModConfigPath(const char* modName) { return RuntimeConfigPath(modName); }
std::string ModLogPath(const char* modName) { return RuntimeLogPath(modName); }

bool IsWindows10OrNewer() {
    // RtlGetVersion 不受兼容性清单影响，能拿到真实版本号
    struct OsVersionInfo {
        unsigned long size;
        unsigned long major;
        unsigned long minor;
        unsigned long build;
        unsigned long platform;
        wchar_t servicePack[128];
    };

    using RtlGetVersionFn = LONG(WINAPI*)(OsVersionInfo*);
    HMODULE module = GetModuleHandleA("ntdll.dll");
    if (!module) return true;
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(module, "RtlGetVersion"));
    if (!rtlGetVersion) return true;

    OsVersionInfo info{};
    info.size = sizeof(info);
    if (rtlGetVersion(&info) != 0) return true;
    return info.major >= 10;
}

bool IsModuleLoaded(const char* moduleName) {
    return moduleName && moduleName[0] && GetModuleHandleW(Utf8ToWide(moduleName).c_str()) != nullptr;
}

bool EnsureDirectory(const std::string& path) {
    if (path.empty()) return false;
    if (CreateDirectoryW(Utf8ToWide(path).c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool DirectoryExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesW(Utf8ToWide(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesW(Utf8ToWide(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DownloadText(const char* url, std::string& output) {
    output.clear();
    if (!url || !url[0]) return false;

    IStream* stream = nullptr;
    if (FAILED(URLOpenBlockingStreamW(nullptr, Utf8ToWide(url).c_str(), &stream, 0, nullptr)) || !stream) {
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
    const std::wstring wide = Utf8ToWide(path);
    if (wide.empty()) return false;

    HANDLE handle = CreateFileW(
        wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    std::string buffer;
    char chunk[4096];
    DWORD bytesRead = 0;
    bool success = true;
    for (;;) {
        if (!ReadFile(handle, chunk, sizeof(chunk), &bytesRead, nullptr)) {
            success = false;
            break;
        }
        if (bytesRead == 0) break;
        buffer.append(chunk, bytesRead);
    }
    CloseHandle(handle);

    if (!success) return false;
    output = std::move(buffer);
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    const std::wstring wide = Utf8ToWide(path);
    if (wide.empty()) return false;

    HANDLE handle = CreateFileW(
        wide.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    const bool ok = WriteFile(handle, content.data(), static_cast<DWORD>(content.size()), &written, nullptr)
        && written == content.size();
    CloseHandle(handle);
    return ok;
}

std::vector<std::string> ListDirectories(const std::string& path) {
    std::vector<std::string> directories;
    if (path.empty()) return directories;

    std::wstring pattern = Utf8ToWide(path);
    if (pattern.empty()) return directories;
    if (pattern.back() != L'\\' && pattern.back() != L'/') pattern += L'\\';
    pattern += L'*';

    WIN32_FIND_DATAW data{};
    HANDLE search = FindFirstFileW(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return directories;

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        const std::string name = WideToUtf8(data.cFileName);
        if (name != "." && name != "..") directories.push_back(name);
    } while (FindNextFileW(search, &data));
    FindClose(search);
    return directories;
}

bool ReadBinaryFile(const std::string& path, std::string& output) {
    std::ifstream stream(XBase::Platform::Utf8ToWide(path).c_str(), std::ios::binary);
    if (!stream.is_open()) return false;

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    output = buffer.str();
    return true;
}

bool WriteBinaryFile(const std::string& path, const std::string& content) {
    std::ofstream stream(XBase::Platform::Utf8ToWide(path).c_str(), std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) return false;

    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return stream.good();
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