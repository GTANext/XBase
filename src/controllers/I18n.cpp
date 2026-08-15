#include <XBase/I18n.h>
#include <XBase/Log.h>
#include <XBase/Json.h>
#include <XBase/Platform.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

namespace {

struct I18nState {
    std::string currentCode;
    std::string directory;
    std::vector<XBase::I18n::LanguageInfo> languages;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dictionaries;
    std::unordered_map<std::string, std::string> fallbackDict;
    bool initialized = false;
};

I18nState& State() {
    static I18nState s;
    return s;
}

std::string ModuleDir() {
    return XBase::Platform::CurrentModuleDirectory();
}

std::vector<std::string> ScanLanguages(const std::string& dir) {
    return XBase::Platform::ListDirectories(dir);
}

bool LoadDictionary(const std::string& filePath, std::unordered_map<std::string, std::string>& dict) {
    auto val = XBase::Json::Value::Load(filePath);
    if (!val.IsObject()) return false;
    const auto& map = std::get<std::unordered_map<std::string, XBase::Json::Value>>(val.data);
    for (const auto& [k, v] : map) {
        if (v.IsString()) dict[k] = v.AsString();
    }
    return true;
}

std::string IndexPath(const std::string& dir, const std::string& code) {
    return dir + code + "\\index.json";
}

std::string FallbackFilePath(const std::string& dir, const std::string& code) {
    return dir + code + ".json";
}

} // namespace

namespace XBase::I18n {

void Init(const std::string& directory) {
    if (State().initialized) return;

    std::string baseDir = directory.empty() ? (ModuleDir() + "XBase\\i18n\\") : directory;
    if (baseDir.back() != '\\') baseDir += '\\';
    State().directory = baseDir;

    auto codes = ScanLanguages(baseDir);
    for (const auto& code : codes) {
        std::string idxPath = IndexPath(baseDir, code);
        auto idxVal = Json::Value::Load(idxPath);
        if (idxVal.IsObject()) {
            std::string name = idxVal["name"].AsString(code);
            State().languages.push_back({code, name});

            auto files = idxVal["files"];
            if (files.IsArray()) {
                const auto& fileArr = std::get<std::vector<Json::Value>>(files.data);
                for (size_t i = 0; i < fileArr.size(); i++) {
                    if (fileArr[i].IsString()) {
                        LoadDictionary(baseDir + code + "\\" + fileArr[i].AsString(), State().dictionaries[code]);
                    }
                }
            } else {
                std::string file = code + ".json";
                LoadDictionary(baseDir + code + "\\" + file, State().dictionaries[code]);
            }
        } else {
            std::string fp = FallbackFilePath(baseDir, code);
            if (LoadDictionary(fp, State().dictionaries[code])) {
                State().languages.push_back({code, code});
            }
        }
    }

    State().currentCode = "zh";
    if (!State().languages.empty()) {
        State().currentCode = State().languages[0].code;
    }

    State().fallbackDict = State().dictionaries["zh"];
    State().initialized = true;
    Log::Info("I18n: " + std::to_string(State().languages.size()) + " languages loaded");
}

void SetLanguage(const std::string& code) {
    if (State().dictionaries.find(code) != State().dictionaries.end()) {
        State().currentCode = code;
    }
}

std::string GetCurrentLanguage() {
    return State().currentCode;
}

std::vector<LanguageInfo> GetAvailableLanguages() {
    return State().languages;
}

const char* T(const char* key) {
    if (!key || !State().initialized) return key ? key : "";

    auto& dict = State().dictionaries[State().currentCode];
    auto it = dict.find(key);
    if (it != dict.end()) return it->second.c_str();

    auto fit = State().fallbackDict.find(key);
    if (fit != State().fallbackDict.end()) return fit->second.c_str();

    return key;
}

const char* Translate(const char* key) {
    return T(key);
}

} // namespace XBase::I18n
