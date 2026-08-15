#include <XBase/Config.h>
#include <XBase/Json.h>
#include <XBase/Log.h>
#include <XBase/Platform.h>

namespace {

struct ConfigState {
    std::string filePath;
    XBase::Json::Value root;
    bool modified = false;
};

ConfigState& State() {
    static ConfigState s;
    return s;
}

std::string DefaultConfigPath() {
    std::string path = XBase::Platform::CurrentModuleDirectory();
    return path + "XBase\\config.json";
}

std::vector<std::string> SplitKey(const std::string& key) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < key.size()) {
        auto dot = key.find('.', start);
        if (dot == std::string::npos) {
            parts.push_back(key.substr(start));
            break;
        }
        parts.push_back(key.substr(start, dot - start));
        start = dot + 1;
    }
    return parts;
}

const XBase::Json::Value* Resolve(const std::string& key) {
    auto parts = SplitKey(key);
    const XBase::Json::Value* cur = &State().root;
    for (const auto& p : parts) {
        if (!cur->IsObject()) return nullptr;
        cur = &(*cur)[p];
        if (cur->IsNull()) return nullptr;
    }
    return cur;
}

XBase::Json::Value* ResolveMutable(const std::string& key) {
    auto parts = SplitKey(key);
    XBase::Json::Value* cur = &State().root;
    for (size_t i = 0; i < parts.size(); i++) {
        if (!cur->IsObject()) {
            *cur = XBase::Json::Value();
            cur->type = XBase::Json::Value::Object;
            cur->data = std::unordered_map<std::string, XBase::Json::Value>();
        }
        auto& map = std::get<std::unordered_map<std::string, XBase::Json::Value>>(cur->data);
        if (i == parts.size() - 1) return &map[parts[i]];
        cur = &map[parts[i]];
    }
    return cur;
}

} // namespace

namespace XBase::Config {

void Init(const std::string& filePath) {
    State().filePath = filePath.empty() ? DefaultConfigPath() : filePath;

    auto dirPos = State().filePath.find_last_of('\\');
    if (dirPos != std::string::npos) {
        std::string dir = State().filePath.substr(0, dirPos);
        XBase::Platform::EnsureDirectory(dir);
    }

    auto loaded = Json::Value::Load(State().filePath);
    if (!loaded.IsNull()) {
        State().root = loaded;
        Log::Info("Config loaded: " + State().filePath);
    } else {
        State().root = Json::Value();
        State().root.type = Json::Value::Object;
        State().root.data = std::unordered_map<std::string, Json::Value>();
        Log::Info("Config created: " + State().filePath);
    }
    State().modified = false;
}

void Save() {
    if (!State().modified) return;
    Json::Value::Save(State().root, State().filePath);
    State().modified = false;
    Log::Info("Config saved: " + State().filePath);
}

const std::string& GetFilePath() {
    return State().filePath;
}

std::string GetString(const std::string& key, const std::string& def) {
    auto* v = Resolve(key);
    return v ? v->AsString(def) : def;
}

int GetInt(const std::string& key, int def) {
    auto* v = Resolve(key);
    return v ? v->AsInt(def) : def;
}

float GetFloat(const std::string& key, float def) {
    auto* v = Resolve(key);
    return v ? static_cast<float>(v->AsNumber(def)) : def;
}

bool GetBool(const std::string& key, bool def) {
    auto* v = Resolve(key);
    return v ? v->AsBool(def) : def;
}

void SetString(const std::string& key, const std::string& value) {
    auto* v = ResolveMutable(key);
    if (v) { *v = Json::Value(value); State().modified = true; }
}

void SetInt(const std::string& key, int value) {
    auto* v = ResolveMutable(key);
    if (v) { *v = Json::Value(value); State().modified = true; }
}

void SetFloat(const std::string& key, float value) {
    auto* v = ResolveMutable(key);
    if (v) { *v = Json::Value(static_cast<double>(value)); State().modified = true; }
}

void SetBool(const std::string& key, bool value) {
    auto* v = ResolveMutable(key);
    if (v) { *v = Json::Value(value); State().modified = true; }
}

bool HasKey(const std::string& key) {
    return Resolve(key) != nullptr;
}

} // namespace XBase::Config
