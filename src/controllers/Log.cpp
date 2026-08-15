#include <XBase/Log.h>
#include <XBase/Platform.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <mutex>
#include <ctime>
#include <exception>
#include <cstdlib>
#include <utility>
#include <Windows.h>

namespace {

static constexpr size_t MAX_ENTRIES = 600;

struct LogState {
    std::ofstream file;
    std::vector<XBase::Log::Entry> entries;
    std::size_t totalCount = 0;
    bool initialized = false;
    std::string filePath;
    std::mutex mtx;
};

LogState& State() {
    static LogState s;
    return s;
}

LPTOP_LEVEL_EXCEPTION_FILTER s_previousExceptionFilter = nullptr;
std::terminate_handler s_previousTerminateHandler = nullptr;
bool s_crashHandlersInstalled = false;

std::string Timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm now;
    localtime_s(&now, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &now);
    return buf;
}

const char* LevelName(XBase::Log::Level level) {
    switch (level) {
    case XBase::Log::Level::Debug: return "DEBUG";
    case XBase::Log::Level::Info:  return "INFO";
    case XBase::Log::Level::Warn:  return "WARN";
    case XBase::Log::Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

bool LooksLikeUtf8(const char* text) {
    if (!text) return true;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
        if (*p <= 0x7F) {
            ++p;
            continue;
        }
        if ((*p & 0xE0) == 0xC0) {
            if ((p[1] & 0xC0) != 0x80) return false;
            p += 2;
            continue;
        }
        if ((*p & 0xF0) == 0xE0) {
            if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
            p += 3;
            continue;
        }
        if ((*p & 0xF8) == 0xF0) {
            if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false;
            p += 4;
            continue;
        }
        return false;
    }
    return true;
}

// 系统 ANSI(ACP) → UTF-8（兼容外部窄字符串）
std::string AcpToUtf8(const char* acp) {
    if (!acp || !acp[0]) return {};
    const int wideLen = MultiByteToWideChar(CP_ACP, 0, acp, -1, nullptr, 0);
    if (wideLen <= 1) return acp;
    std::wstring wide(static_cast<std::size_t>(wideLen - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, acp, -1, wide.data(), wideLen);
    return XBase::Platform::WideToUtf8(wide);
}

// 日志消息统一成 UTF-8：合法 UTF-8 原样保留，否则按系统 ACP 转码
std::string NormalizeToUtf8(const char* message) {
    if (!message) return {};
    if (LooksLikeUtf8(message)) return message;
    return AcpToUtf8(message);
}

std::string GetDefaultLogPath() {
    const std::string path = XBase::Platform::CurrentModuleDirectory();
    return path + "XBase\\logs\\xbase.log";
}

bool EnsureDir(const std::string& path) {
    auto pos = path.find_last_of('\\');
    if (pos == std::string::npos) return false;
    std::string dir = path.substr(0, pos);
    return XBase::Platform::EnsureDirectory(dir);
}

void WriteUnlocked(XBase::Log::Level level, const char* message) {
    if (!State().initialized) return;

    std::string ts = Timestamp();
    const std::string utf8Message = NormalizeToUtf8(message);
    std::string line = "[" + ts + "] [" + LevelName(level) + "] " + utf8Message;

    if (State().file.is_open()) {
        // binary 模式逐字节写入，避免 text 模式对 \n 及本地化码页做转换
        State().file.write(line.data(), static_cast<std::streamsize>(line.size()));
        State().file.put('\n');
        State().file.flush();
    }

    XBase::Log::Entry entry;
    entry.level = level;
    entry.text = utf8Message;
    entry.timestamp = ts;
    State().entries.push_back(std::move(entry));
    if (State().entries.size() > MAX_ENTRIES) {
        State().entries.erase(State().entries.begin());
    }
    State().totalCount++;
}

LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* exceptionInfo) {
    if (State().initialized && exceptionInfo && exceptionInfo->ExceptionRecord) {
        std::ostringstream message;
        message << "Unhandled exception code=0x" << std::hex << std::uppercase
                << exceptionInfo->ExceptionRecord->ExceptionCode
                << " address=0x"
                << reinterpret_cast<std::uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionAddress)
                << " thread=" << std::dec << GetCurrentThreadId();
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(exceptionInfo->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi))
            && mbi.AllocationBase) {
            char moduleName[MAX_PATH] = {};
            if (GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), moduleName, MAX_PATH)) {
                const char* name = std::strrchr(moduleName, '\\');
                message << " module=" << (name ? name + 1 : moduleName)
                        << " base=0x" << std::hex << std::uppercase
                        << reinterpret_cast<std::uintptr_t>(mbi.AllocationBase);
            }
        }
        std::lock_guard<std::mutex> lock(State().mtx);
        WriteUnlocked(XBase::Log::Level::Error, message.str().c_str());
    }
    return s_previousExceptionFilter
        ? s_previousExceptionFilter(exceptionInfo)
        : EXCEPTION_CONTINUE_SEARCH;
}

void HandleTerminate() {
    {
        std::lock_guard<std::mutex> lock(State().mtx);
        WriteUnlocked(XBase::Log::Level::Error, "std::terminate invoked");
    }
    if (s_previousTerminateHandler) {
        s_previousTerminateHandler();
        return;
    }
    std::abort();
}

void InstallCrashHandlers() {
    if (s_crashHandlersInstalled) return;
    s_previousExceptionFilter = SetUnhandledExceptionFilter(HandleUnhandledException);
    s_previousTerminateHandler = std::set_terminate(HandleTerminate);
    s_crashHandlersInstalled = true;
}

} // namespace

namespace XBase::Log {

void Init(const char* filePath) {
    std::lock_guard<std::mutex> lock(State().mtx);
    if (State().initialized) return;

    State().filePath = filePath ? filePath : GetDefaultLogPath();
    EnsureDir(State().filePath);

    // 每次启动都清空上次日志；binary + UTF-8 BOM，保证任何编辑器都能正确显示
    const std::wstring widePath = XBase::Platform::Utf8ToWide(State().filePath);
    State().file.open(std::filesystem::path(widePath), std::ios::binary | std::ios::out | std::ios::trunc);
    State().initialized = true;
    if (State().file.is_open()) {
        const char bom[] = { '\xEF', '\xBB', '\xBF' };
        State().file.write(bom, sizeof(bom));
        State().file.flush();
    }
    InstallCrashHandlers();
    WriteUnlocked(Level::Info, "XBase Log initialized");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(State().mtx);
    if (!State().initialized) return;
    WriteUnlocked(Level::Info, "XBase Log shutdown");
    if (s_crashHandlersInstalled) {
        SetUnhandledExceptionFilter(s_previousExceptionFilter);
        std::set_terminate(s_previousTerminateHandler);
        s_previousExceptionFilter = nullptr;
        s_previousTerminateHandler = nullptr;
        s_crashHandlersInstalled = false;
    }
    State().file.close();
    State().initialized = false;
}

bool IsInitialized() {
    std::lock_guard<std::mutex> lock(State().mtx);
    return State().initialized;
}

void Write(Level level, const char* message) {
    std::lock_guard<std::mutex> lock(State().mtx);
    WriteUnlocked(level, message);
}

void Write(Level level, const std::string& message) {
    Write(level, message.c_str());
}

void Debug(const char* message) { Write(Level::Debug, message); }
void Info(const char* message)  { Write(Level::Info, message); }
void Warn(const char* message)  { Write(Level::Warn, message); }
void Error(const char* message) { Write(Level::Error, message); }

void Debug(const std::string& message) { Write(Level::Debug, message); }
void Info(const std::string& message)  { Write(Level::Info, message); }
void Warn(const std::string& message)  { Write(Level::Warn, message); }
void Error(const std::string& message) { Write(Level::Error, message); }

std::vector<Entry> GetEntries() {
    std::lock_guard<std::mutex> lock(State().mtx);
    return State().entries;
}

std::size_t GetTotalCount() {
    std::lock_guard<std::mutex> lock(State().mtx);
    return State().totalCount;
}

std::string GetText() {
    std::lock_guard<std::mutex> lock(State().mtx);
    std::ostringstream oss;
    for (const auto& e : State().entries) {
        oss << "[" << e.timestamp << "] [" << LevelName(e.level) << "] " << e.text << "\n";
    }
    return oss.str();
}

std::string GetLogFilePath() {
    std::lock_guard<std::mutex> lock(State().mtx);
    return State().filePath;
}

} // namespace XBase::Log
