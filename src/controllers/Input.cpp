#include <XBase/Input.h>

#include "InputInternal.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>

namespace {

constexpr std::size_t KeyCount = static_cast<std::size_t>(XBase::Input::Key::Z) + 1;
std::array<std::atomic<bool>, KeyCount> s_down{};
std::array<std::atomic<bool>, KeyCount> s_pressed{};
std::array<std::atomic<XBase::Input::Modifiers>, KeyCount> s_pressedModifiers{};
std::atomic<XBase::Input::Modifiers> s_modifiers{0};

std::size_t Index(XBase::Input::Key key) {
    return static_cast<std::size_t>(key);
}

XBase::Input::Key FromVirtualKey(std::uint32_t key) {
    using XBase::Input::Key;
    if (key >= '0' && key <= '9') {
        return static_cast<Key>(static_cast<unsigned int>(Key::Digit0) + key - '0');
    }
    if (key >= 'A' && key <= 'Z') {
        return static_cast<Key>(static_cast<unsigned int>(Key::A) + key - 'A');
    }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        return static_cast<Key>(static_cast<unsigned int>(Key::Num0) + key - VK_NUMPAD0);
    }
    if (key >= VK_F1 && key <= VK_F12) {
        return static_cast<Key>(static_cast<unsigned int>(Key::F1) + key - VK_F1);
    }

    switch (key) {
    case VK_BACK: return Key::Backspace;
    case VK_TAB: return Key::Tab;
    case VK_RETURN: return Key::Enter;
    case VK_ESCAPE: return Key::Escape;
    case VK_SPACE: return Key::Space;
    case VK_PRIOR: return Key::PageUp;
    case VK_NEXT: return Key::PageDown;
    case VK_END: return Key::End;
    case VK_HOME: return Key::Home;
    case VK_LEFT: return Key::Left;
    case VK_UP: return Key::Up;
    case VK_RIGHT: return Key::Right;
    case VK_DOWN: return Key::Down;
    case VK_INSERT: return Key::Insert;
    case VK_DELETE: return Key::Delete;
    case VK_MULTIPLY: return Key::Multiply;
    case VK_ADD: return Key::Add;
    case VK_SUBTRACT: return Key::Subtract;
    case VK_DECIMAL: return Key::Decimal;
    case VK_DIVIDE: return Key::Divide;
    default: return Key::None;
    }
}

XBase::Input::Modifier ModifierFromVirtualKey(std::uint32_t key) {
    using XBase::Input::Modifier;
    switch (key) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return Modifier::Ctrl;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return Modifier::Alt;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return Modifier::Shift;
    default:
        return Modifier::None;
    }
}

bool HasModifiers(XBase::Input::Modifiers required, XBase::Input::Modifiers actual) {
    return (actual & required) == required;
}

bool ConsumePressed(XBase::Input::Key key, XBase::Input::Modifiers* modifiers = nullptr) {
    if (key == XBase::Input::Key::None) return false;
    const std::size_t index = Index(key);
    if (!s_pressed[index].exchange(false, std::memory_order_acq_rel)) return false;
    if (modifiers) {
        *modifiers = s_pressedModifiers[index].load(std::memory_order_acquire);
    }
    return true;
}

} // namespace

namespace XBase::Detail::Input {

void HandleVirtualKey(std::uint32_t virtualKey, bool down, bool repeat) {
    const XBase::Input::Modifier modifier = ModifierFromVirtualKey(virtualKey);
    if (modifier != XBase::Input::Modifier::None) {
        const XBase::Input::Modifiers modifierBit = XBase::Input::ModifierBit(modifier);
        if (down) s_modifiers.fetch_or(modifierBit, std::memory_order_acq_rel);
        else s_modifiers.fetch_and(static_cast<XBase::Input::Modifiers>(~modifierBit), std::memory_order_acq_rel);
        return;
    }

    const XBase::Input::Key key = FromVirtualKey(virtualKey);
    if (key == XBase::Input::Key::None) return;
    const std::size_t index = Index(key);
    s_down[index].store(down, std::memory_order_release);
    if (down && !repeat) {
        s_pressedModifiers[index].store(s_modifiers.load(std::memory_order_acquire), std::memory_order_release);
        s_pressed[index].store(true, std::memory_order_release);
    }
}

void Reset() {
    s_modifiers.store(0, std::memory_order_release);
    for (auto& state : s_down) state.store(false, std::memory_order_release);
    for (auto& state : s_pressed) state.store(false, std::memory_order_release);
    for (auto& modifiers : s_pressedModifiers) modifiers.store(0, std::memory_order_release);
}

} // namespace XBase::Detail::Input

namespace XBase::Input {

bool IsDown(Key key) {
    if (key == Key::None) return false;
    return s_down[Index(key)].load(std::memory_order_acquire);
}

bool IsModifierDown(Modifier modifier) {
    if (modifier == Modifier::None) return false;
    return (s_modifiers.load(std::memory_order_acquire) & ModifierBit(modifier)) != 0;
}

bool WasPressed(Key key) {
    return ConsumePressed(key);
}

bool IsDown(const Hotkey& hotkey) {
    return IsDown(hotkey.key)
        && HasModifiers(hotkey.modifiers, s_modifiers.load(std::memory_order_acquire));
}

bool WasPressed(const Hotkey& hotkey) {
    Modifiers modifiers = 0;
    return ConsumePressed(hotkey.key, &modifiers)
        && HasModifiers(hotkey.modifiers, modifiers);
}

bool CapturePressedHotkey(Hotkey& hotkey, bool allowClear) {
    if (allowClear && (WasPressed(Key::Backspace) || WasPressed(Key::Delete))) {
        hotkey = {};
        return true;
    }
    for (std::size_t index = Index(Key::Tab); index < KeyCount; ++index) {
        const Key key = static_cast<Key>(index);
        Modifiers modifiers = 0;
        if (!ConsumePressed(key, &modifiers)) continue;
        hotkey.key = key;
        hotkey.modifiers = modifiers;
        return true;
    }
    return false;
}

std::string FormatHotkey(const Hotkey& hotkey) {
    if (hotkey.key == Key::None) return "None";

    std::string result;
    if (hotkey.modifiers & ModifierBit(Modifier::Ctrl)) result += "Ctrl+";
    if (hotkey.modifiers & ModifierBit(Modifier::Alt)) result += "Alt+";
    if (hotkey.modifiers & ModifierBit(Modifier::Shift)) result += "Shift+";
    result += GetKeyName(hotkey.key);
    return result;
}

bool ParseHotkey(const std::string& value, Hotkey& hotkey) {
    hotkey = {};
    std::size_t start = 0;
    bool hasKey = false;

    while (start <= value.size()) {
        const std::size_t end = value.find('+', start);
        std::string part = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front()))) part.erase(part.begin());
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back()))) part.pop_back();
        for (char& character : part) {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }

        if (part == "CTRL" || part == "CONTROL" || part == "LCTRL" || part == "RCTRL") {
            hotkey.modifiers |= ModifierBit(Modifier::Ctrl);
        } else if (part == "ALT" || part == "MENU" || part == "LALT" || part == "RALT") {
            hotkey.modifiers |= ModifierBit(Modifier::Alt);
        } else if (part == "SHIFT" || part == "LSHIFT" || part == "RSHIFT") {
            hotkey.modifiers |= ModifierBit(Modifier::Shift);
        } else if (part == "NONE" || part == "DISABLED") {
            return value.find('+') == std::string::npos;
        } else {
            if (hasKey) return false;
            for (std::size_t index = Index(Key::Backspace); index < KeyCount; ++index) {
                const Key candidate = static_cast<Key>(index);
                if (part == GetKeyName(candidate)
                    || (part == "BACK" && candidate == Key::Backspace)
                    || (part == "RETURN" && candidate == Key::Enter)
                    || (part == "ESCAPE" && candidate == Key::Escape)
                    || (part == "PGUP" && candidate == Key::PageUp)
                    || (part == "PGDN" && candidate == Key::PageDown)
                    || (part == "INS" && candidate == Key::Insert)
                    || (part == "DEL" && candidate == Key::Delete)) {
                    hotkey.key = candidate;
                    hasKey = true;
                    break;
                }
            }
            if (!hasKey) return false;
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    return hasKey || hotkey.key == Key::None;
}

const char* GetKeyName(Key key) {
    static constexpr const char* Names[] = {
        "None", "BACKSPACE", "TAB", "ENTER", "ESC", "SPACE", "PAGEUP", "PAGEDOWN",
        "END", "HOME", "LEFT", "UP", "RIGHT", "DOWN", "INSERT", "DELETE",
        "NUM0", "NUM1", "NUM2", "NUM3", "NUM4", "NUM5", "NUM6", "NUM7", "NUM8", "NUM9",
        "MULTIPLY", "ADD", "SUBTRACT", "DECIMAL", "DIVIDE",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
    };
    const std::size_t index = Index(key);
    return index < std::size(Names) ? Names[index] : "None";
}

} // namespace XBase::Input