#include <XBase/Log.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <ctime>
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

std::string GetDefaultLogPath() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of('\\');
    if (pos != std::string::npos) path.resize(pos + 1);
    return path + "XBase\\logs\\xbase.log";
}

bool EnsureDir(const std::string& path) {
    auto pos = path.find_last_of('\\');
    if (pos == std::string::npos) return false;
    std::string dir = path.substr(0, pos);
    CreateDirectoryA(dir.c_str(), nullptr);
    return true;
}

void WriteUnlocked(XBase::Log::Level level, const char* message) {
    if (!State().initialized) return;

    std::string ts = Timestamp();
    std::string line = "[" + ts + "] [" + LevelName(level) + "] " + message;

    if (State().file.is_open()) {
        State().file << line << std::endl;
        State().file.flush();
    }

    XBase::Log::Entry entry;
    entry.level = level;
    entry.text = message;
    entry.timestamp = ts;
    State().entries.push_back(std::move(entry));
    if (State().entries.size() > MAX_ENTRIES) {
        State().entries.erase(State().entries.begin());
    }
    State().totalCount++;
}

} // namespace

namespace XBase::Log {

void Init(const char* filePath) {
    std::lock_guard<std::mutex> lock(State().mtx);
    if (State().initialized) return;

    State().filePath = filePath ? filePath : GetDefaultLogPath();
    EnsureDir(State().filePath);

    State().file.open(State().filePath, std::ios::app);
    State().initialized = true;
    WriteUnlocked(Level::Info, "XBase Log initialized");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(State().mtx);
    if (!State().initialized) return;
    WriteUnlocked(Level::Info, "XBase Log shutdown");
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
