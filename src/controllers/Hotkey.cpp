#include <XBase/Hotkey.h>

#include <XBase/Json.h>
#include <XBase/Platform.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using XBase::Hotkey::Binding;
using XBase::Hotkey::Key;
using XBase::Hotkey::Mode;
using XBase::Hotkey::Modifiers;

constexpr std::size_t kVirtualKeyCount = 256;

std::unordered_map<std::string, std::unique_ptr<Binding>> s_bindings;
std::unordered_map<std::string, Binding> s_persisted;
std::array<bool, kVirtualKeyCount> s_keyDown{};
std::array<bool, kVirtualKeyCount> s_keyPressed{};

struct KeyName {
    Key key;
    const char* name;
};

constexpr KeyName kKeyNames[] = {
    {Key::LButton, "LMB"}, {Key::RButton, "RMB"}, {Key::MButton, "MMB"},
    {Key::Back, "Backspace"}, {Key::Tab, "Tab"}, {Key::Clear, "Clear"},
    {Key::Return, "Enter"}, {Key::Shift, "Shift"}, {Key::Control, "Control"},
    {Key::Menu, "Alt"}, {Key::Pause, "Pause"}, {Key::Capital, "CapsLock"},
    {Key::Escape, "Escape"}, {Key::Space, "Space"}, {Key::Prior, "PageUp"},
    {Key::Next, "PageDown"}, {Key::End, "End"}, {Key::Home, "Home"},
    {Key::Left, "Left"}, {Key::Up, "Up"}, {Key::Right, "Right"}, {Key::Down, "Down"},
    {Key::Select, "Select"}, {Key::Print, "Print"}, {Key::Execute, "Execute"},
    {Key::Snapshot, "PrintScreen"}, {Key::Insert, "Insert"}, {Key::Delete, "Delete"},
    {Key::Help, "Help"},
    {Key::D0, "0"}, {Key::D1, "1"}, {Key::D2, "2"}, {Key::D3, "3"}, {Key::D4, "4"},
    {Key::D5, "5"}, {Key::D6, "6"}, {Key::D7, "7"}, {Key::D8, "8"}, {Key::D9, "9"},
    {Key::A, "A"}, {Key::B, "B"}, {Key::C, "C"}, {Key::D, "D"}, {Key::E, "E"},
    {Key::F, "F"}, {Key::G, "G"}, {Key::H, "H"}, {Key::I, "I"}, {Key::J, "J"},
    {Key::K, "K"}, {Key::L, "L"}, {Key::M, "M"}, {Key::N, "N"}, {Key::O, "O"},
    {Key::P, "P"}, {Key::Q, "Q"}, {Key::R, "R"}, {Key::S, "S"}, {Key::T, "T"},
    {Key::U, "U"}, {Key::V, "V"}, {Key::W, "W"}, {Key::X, "X"}, {Key::Y, "Y"},
    {Key::Z, "Z"},
    {Key::LWin, "LWin"}, {Key::RWin, "RWin"}, {Key::Apps, "Apps"},
    {Key::Sleep, "Sleep"},
    {Key::Numpad0, "Num0"}, {Key::Numpad1, "Num1"}, {Key::Numpad2, "Num2"},
    {Key::Numpad3, "Num3"}, {Key::Numpad4, "Num4"}, {Key::Numpad5, "Num5"},
    {Key::Numpad6, "Num6"}, {Key::Numpad7, "Num7"}, {Key::Numpad8, "Num8"},
    {Key::Numpad9, "Num9"},
    {Key::Multiply, "Multiply"}, {Key::Add, "Add"}, {Key::Separator, "Separator"},
    {Key::Subtract, "Subtract"}, {Key::Decimal, "Decimal"}, {Key::Divide, "Divide"},
    {Key::F1, "F1"}, {Key::F2, "F2"}, {Key::F3, "F3"}, {Key::F4, "F4"},
    {Key::F5, "F5"}, {Key::F6, "F6"}, {Key::F7, "F7"}, {Key::F8, "F8"},
    {Key::F9, "F9"}, {Key::F10, "F10"}, {Key::F11, "F11"}, {Key::F12, "F12"},
    {Key::F13, "F13"}, {Key::F14, "F14"}, {Key::F15, "F15"}, {Key::F16, "F16"},
    {Key::F17, "F17"}, {Key::F18, "F18"}, {Key::F19, "F19"}, {Key::F20, "F20"},
    {Key::F21, "F21"}, {Key::F22, "F22"}, {Key::F23, "F23"}, {Key::F24, "F24"},
    {Key::NumLock, "NumLock"}, {Key::Scroll, "ScrollLock"},
    {Key::LShift, "LShift"}, {Key::RShift, "RShift"},
    {Key::LControl, "LControl"}, {Key::RControl, "RControl"},
    {Key::LMenu, "LAlt"}, {Key::RMenu, "RAlt"},
    {Key::Oem1, "Semicolon"}, {Key::OemPlus, "Plus"}, {Key::OemComma, "Comma"},
    {Key::OemMinus, "Minus"}, {Key::OemPeriod, "Period"}, {Key::Oem2, "Slash"},
    {Key::Oem3, "Tilde"}, {Key::Oem4, "BracketLeft"}, {Key::Oem5, "Backslash"},
    {Key::Oem6, "BracketRight"}, {Key::Oem7, "Quote"},
};

bool IsPhysicalDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsVirtualKeyDown(int virtualKey) {
    if (virtualKey <= 0 || virtualKey >= static_cast<int>(kVirtualKeyCount)) return false;
    return s_keyDown[static_cast<std::size_t>(virtualKey)];
}

bool IsVirtualKeyPressed(int virtualKey) {
    if (virtualKey <= 0 || virtualKey >= static_cast<int>(kVirtualKeyCount)) return false;
    return s_keyPressed[static_cast<std::size_t>(virtualKey)];
}

bool HasModifiers(const Modifiers& required) {
    if (required.ctrl && !IsPhysicalDown(VK_CONTROL)) return false;
    if (required.shift && !IsPhysicalDown(VK_SHIFT)) return false;
    if (required.alt && !IsPhysicalDown(VK_MENU)) return false;
    if (required.win && !IsPhysicalDown(VK_LWIN) && !IsPhysicalDown(VK_RWIN)) return false;
    return true;
}

bool IsBindingDown(const Binding& binding) {
    return IsVirtualKeyDown(static_cast<int>(binding.key)) && HasModifiers(binding.mods);
}

// ?????????? asi ???????????????
void MigrationHook(const char* fileName) {
    const std::string target = XBase::Platform::XBaseDirectory() + fileName;
    if (XBase::Platform::FileExists(target)) return;

    const std::string legacy = XBase::Platform::CurrentModuleDirectory() + "XBase\\" + fileName;
    if (!XBase::Platform::FileExists(legacy)) return;

    std::string content;
    if (XBase::Platform::ReadTextFile(legacy, content)) {
        XBase::Platform::EnsureDirectory(XBase::Platform::XBaseDirectory());
        XBase::Platform::WriteTextFile(target, content);
    }
}

std::string DefaultBindingsPath() {
    return XBase::Platform::XBaseDirectory() + "hotkeys.json";
}

const char* ModeName(Mode mode) {
    switch (mode) {
    case Mode::Hold: return "hold";
    case Mode::Once: return "once";
    case Mode::Toggle:
    default: return "toggle";
    }
}

Mode ModeFromName(const std::string& name) {
    if (name == "hold") return Mode::Hold;
    if (name == "once") return Mode::Once;
    return Mode::Toggle;
}

std::string ToUpper(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string TrimSpaces(const std::string& value) {
    std::size_t start = 0;
    std::size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

void ApplyPersisted(const std::string& id, const Binding& persisted) {
    Binding* binding = XBase::Hotkey::Get(id);
    if (!binding) return;
    binding->key = persisted.key;
    binding->mods = persisted.mods;
    binding->mode = persisted.mode;
}

void DispatchBinding(Binding& binding) {
    const bool down = IsBindingDown(binding);
    const bool wasDown = binding.prevPressed;
    binding.prevPressed = down;

    switch (binding.mode) {
    case Mode::Toggle:
        if (down && !wasDown) {
            binding.state = !binding.state;
            if (binding.onChange) binding.onChange(binding.state);
        }
        return;
    case Mode::Hold:
        if (down != binding.state) {
            binding.state = down;
            if (binding.onChange) binding.onChange(binding.state);
        }
        return;
    case Mode::Once:
        if (down && !wasDown && binding.onChange) binding.onChange(true);
        return;
    }
}

} // namespace

namespace XBase::Hotkey {

void Init() {
    for (std::size_t virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        s_keyDown[virtualKey] = IsPhysicalDown(static_cast<int>(virtualKey));
        s_keyPressed[virtualKey] = false;
    }
    for (auto& entry : s_bindings) {
        entry.second->state = false;
        entry.second->prevPressed = IsBindingDown(*entry.second);
    }
}

void Process() {
    for (std::size_t virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        const bool down = IsPhysicalDown(static_cast<int>(virtualKey));
        s_keyPressed[virtualKey] = down && !s_keyDown[virtualKey];
        s_keyDown[virtualKey] = down;
    }

    for (auto& entry : s_bindings) {
        DispatchBinding(*entry.second);
    }
}

bool Register(const std::string& id, Key key, Modifiers mods, Mode mode, std::function<void(bool)> callback) {
    if (id.empty()) return false;

    auto binding = std::make_unique<Binding>();
    binding->id = id;
    binding->key = key;
    binding->mods = mods;
    binding->mode = mode;
    binding->onChange = std::move(callback);

    const auto persisted = s_persisted.find(id);
    if (persisted != s_persisted.end()) {
        binding->key = persisted->second.key;
        binding->mods = persisted->second.mods;
        binding->mode = persisted->second.mode;
    }
    binding->prevPressed = IsBindingDown(*binding);

    s_bindings[id] = std::move(binding);
    return true;
}

bool Unregister(const std::string& id) {
    return s_bindings.erase(id) > 0;
}

Binding* Get(const std::string& id) {
    const auto found = s_bindings.find(id);
    return found == s_bindings.end() ? nullptr : found->second.get();
}

std::vector<Binding*> GetAll() {
    std::vector<Binding*> bindings;
    bindings.reserve(s_bindings.size());
    for (auto& entry : s_bindings) {
        bindings.push_back(entry.second.get());
    }
    return bindings;
}

bool IsPressed(Key key) {
    return IsVirtualKeyPressed(static_cast<int>(key));
}

bool IsKeyDown(Key key) {
    return IsVirtualKeyDown(static_cast<int>(key));
}

bool IsToggled(Key key) {
    for (const auto& entry : s_bindings) {
        const Binding& binding = *entry.second;
        if (binding.key == key && binding.mode == Mode::Toggle && binding.state) return true;
    }
    return false;
}

void SaveBindings(const std::string& filePath) {
    const std::string path = filePath.empty() ? DefaultBindingsPath() : filePath;

    Json::Value bindings;
    bindings.type = Json::Value::Object;
    bindings.data = std::unordered_map<std::string, Json::Value>();

    for (const auto& entry : s_bindings) {
        const Binding& binding = *entry.second;
        Json::Value item;
        item.type = Json::Value::Object;
        item.data = std::unordered_map<std::string, Json::Value>();
        item.Set("key", Json::Value(static_cast<int>(binding.key)));
        item.Set("ctrl", Json::Value(binding.mods.ctrl));
        item.Set("shift", Json::Value(binding.mods.shift));
        item.Set("alt", Json::Value(binding.mods.alt));
        item.Set("win", Json::Value(binding.mods.win));
        item.Set("mode", Json::Value(ModeName(binding.mode)));
        bindings.Set(entry.first, item);
    }

    Json::Value root;
    root.type = Json::Value::Object;
    root.data = std::unordered_map<std::string, Json::Value>();
    root.Set("bindings", bindings);
    Json::Value::Save(root, path);
}

void LoadBindings(const std::string& filePath) {
    const std::string path = filePath.empty() ? DefaultBindingsPath() : filePath;
    if (filePath.empty()) MigrationHook("hotkeys.json");
    const Json::Value root = Json::Value::Load(path);
    if (!root.IsObject()) return;

    const Json::Value& bindings = root["bindings"];
    if (!bindings.IsObject()) return;

    const auto& items = std::get<std::unordered_map<std::string, Json::Value>>(bindings.data);
    for (const auto& entry : items) {
        const Json::Value& item = entry.second;
        if (!item.IsObject()) continue;

        Binding persisted;
        persisted.id = entry.first;
        persisted.key = static_cast<Key>(std::clamp(item["key"].AsInt(0), 0, 255));
        persisted.mods.ctrl = item["ctrl"].AsBool(false);
        persisted.mods.shift = item["shift"].AsBool(false);
        persisted.mods.alt = item["alt"].AsBool(false);
        persisted.mods.win = item["win"].AsBool(false);
        persisted.mode = ModeFromName(item["mode"].AsString("toggle"));

        s_persisted[entry.first] = persisted;
        ApplyPersisted(entry.first, persisted);
    }
}

std::string KeyToString(Key key) {
    for (const KeyName& entry : kKeyNames) {
        if (entry.key == key) return entry.name;
    }
    if (key == Key::None) return "None";

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "VK_0x%02X", static_cast<unsigned int>(key));
    return buffer;
}

Key StringToKey(const std::string& str) {
    const std::string value = ToUpper(TrimSpaces(str));
    if (value.empty() || value == "NONE" || value == "DISABLED") return Key::None;

    for (const KeyName& entry : kKeyNames) {
        if (value == ToUpper(entry.name)) return entry.key;
    }

    char* end = nullptr;
    const long numeric = std::strtol(value.c_str(), &end, 0);
    if (end && end != value.c_str() && *end == '\0' && numeric > 0 && numeric < 256) {
        return static_cast<Key>(numeric);
    }

    if (value.size() == 1) {
        const unsigned char character = static_cast<unsigned char>(value[0]);
        if ((character >= '0' && character <= '9') || (character >= 'A' && character <= 'Z')) {
            return static_cast<Key>(character);
        }
    }
    return Key::None;
}

} // namespace XBase::Hotkey
