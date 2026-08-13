#include <XBase/UI.h>
#include <XBase/Hooks.h>
#include <XBase/Log.h>
#include <XBase/Theme.h>

#include <imgui.h>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace XBase::UI {
namespace {

ImVec4 ToImVec4(Color color) {
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

ImVec4 ToImVec4(ColorF color) {
    return ImVec4(color.r, color.g, color.b, color.a);
}

ColorF ToColorF(Color color) {
    return ColorF(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

ImU32 ToImGuiColor(const ImVec4& color) {
    return ImGui::ColorConvertFloat4ToU32(color);
}

struct WindowEntry {
    std::string name;
    DrawFn drawFn;
    bool open = false;
    WindowFlags flags = 0;
};

struct TabItem {
    std::string label;
    DrawFn drawFn;
};

std::string g_title = "XBase";
bool g_initialized = false;
std::vector<WindowEntry> g_windows;
std::string g_tabBarName;
std::vector<TabItem> g_tabItems;
int g_activeTab = 0;
Hooks::DrawCallbackId g_drawCallbackId;

constexpr float kMenuSurfaceWidth = 432.0f;
constexpr float kMenuSurfaceHeaderHeight = 52.0f;
constexpr float kMenuSurfaceFooterHeight = 30.0f;
constexpr float kMenuSurfaceRowHeight = 38.0f;
constexpr float kMenuSurfacePaddingX = 14.0f;
constexpr float kMenuSurfaceAccentWidth = 4.0f;

struct MenuSurfaceTransientState {
    int flashIndex = -1;
    double flashUntil = 0.0;
};

struct MenuSurfaceContext {
    MenuSurfaceState* state = nullptr;
    MenuSurfaceTransientState* transient = nullptr;
    bool allowMouse = false;
    bool actionConsumed = false;
    bool backRequested = false;
    bool selectionCanAdjust = false;
    bool scrollRequested = false;
    int currentIndex = 0;
};

struct MenuSurfaceRowResult {
    bool activated = false;
    bool stepLeft = false;
    bool stepRight = false;
};

MenuSurfaceContext* g_menuSurface = nullptr;
std::unordered_map<std::string, MenuSurfaceTransientState> g_menuSurfaceStates;
bool g_tabsOpen = false;

ImU32 ToImGuiColor(ColorF color) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
}

void ClampMenuSurfaceSelection(MenuSurfaceState& state) {
    if (state.itemCount <= 0) {
        state.selectedIndex = 0;
        return;
    }
    while (state.selectedIndex < 0) state.selectedIndex += state.itemCount;
    while (state.selectedIndex >= state.itemCount) state.selectedIndex -= state.itemCount;
}

void MoveMenuSurfaceSelection(MenuSurfaceContext& context, int delta) {
    if (!context.state || context.state->itemCount <= 0) return;
    context.state->selectedIndex += delta;
    ClampMenuSurfaceSelection(*context.state);
    context.scrollRequested = true;
}

bool MenuSurfacePressed(ImGuiKey primary, ImGuiKey gamepad, ImGuiKey keypad, bool repeat) {
    return ImGui::IsKeyPressed(primary, repeat)
        || ImGui::IsKeyPressed(gamepad, repeat)
        || ImGui::IsKeyPressed(keypad, repeat);
}

MenuSurfaceRowResult DrawMenuSurfaceRow(const char* label, const char* rightText, bool canAdjust) {
    MenuSurfaceRowResult result{};
    if (!g_menuSurface || !g_menuSurface->state) return result;

    MenuSurfaceContext& context = *g_menuSurface;
    const int index = context.currentIndex++;
    bool selected = index == context.state->selectedIndex;
    const bool flashing = context.transient
        && index == context.transient->flashIndex
        && ImGui::GetTime() < context.transient->flashUntil;
    if (selected && canAdjust) context.selectionCanAdjust = true;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 rowMax(rowMin.x + width, rowMin.y + kMenuSurfaceRowHeight);

    ImGui::PushID(index);
    ImGui::InvisibleButton("##menu-surface-row", ImVec2(width, kMenuSurfaceRowHeight));
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool leftClick = !disabled && context.allowMouse && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (context.allowMouse && hovered) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            context.state->selectedIndex = index;
            selected = true;
        }
    }

    ImVec2 rightPosition{};
    ImVec2 rightSize{};
    if (rightText && rightText[0]) {
        rightSize = ImGui::CalcTextSize(rightText);
        rightPosition = ImVec2(
            rowMax.x - kMenuSurfacePaddingX - rightSize.x,
            rowMin.y + (kMenuSurfaceRowHeight - rightSize.y) * 0.5f);
    }

    if (leftClick) {
        context.state->selectedIndex = index;
        selected = true;
        if (canAdjust && rightText && rightText[0] && ImGui::GetIO().MousePos.x >= rightPosition.x - 10.0f) {
            const float middle = rightPosition.x + rightSize.x * 0.5f;
            result.stepLeft = ImGui::GetIO().MousePos.x < middle;
            result.stepRight = !result.stepLeft;
        } else {
            result.activated = true;
        }
    }

    const ImU32 accent = ToImGuiColor(Theme::GetColors().primary);
    if (selected || flashing) {
        drawList->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, flashing ? 220 : 255));
        drawList->AddRectFilled(rowMin, ImVec2(rowMin.x + kMenuSurfaceAccentWidth, rowMax.y), accent);
    } else if (context.allowMouse && hovered) {
        drawList->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 28));
    }

    const ImU32 textColor = selected || flashing ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 230);
    ImU32 rightColor = selected || flashing ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 160);
    if (!selected && !flashing && rightText) {
        if (std::strcmp(rightText, "ON") == 0) rightColor = IM_COL32(114, 204, 114, 255);
        if (std::strcmp(rightText, "OFF") == 0) rightColor = IM_COL32(255, 255, 255, 110);
    }

    const ImVec2 labelPosition(
        rowMin.x + kMenuSurfacePaddingX + (selected || flashing ? 2.0f : 0.0f),
        rowMin.y + (kMenuSurfaceRowHeight - ImGui::GetTextLineHeight()) * 0.5f);
    drawList->AddText(labelPosition, textColor, label ? label : "");
    if (rightText && rightText[0]) drawList->AddText(rightPosition, rightColor, rightText);

    if (!disabled && selected && !context.actionConsumed) {
        if (canAdjust) {
            if (MenuSurfacePressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_Keypad4, true)) {
                result.stepLeft = true;
                context.actionConsumed = true;
            } else if (MenuSurfacePressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_Keypad6, true)) {
                result.stepRight = true;
                context.actionConsumed = true;
            }
        } else if (MenuSurfacePressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_Keypad4, false)) {
            context.backRequested = true;
            context.actionConsumed = true;
        }

        const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter, false)
            || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)
            || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)
            || ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)
            || ImGui::IsKeyPressed(ImGuiKey_Space, false);
        if (confirm && !context.actionConsumed) {
            result.activated = true;
            context.actionConsumed = true;
        }
    }

    if ((result.activated || result.stepLeft || result.stepRight) && context.transient) {
        context.transient->flashIndex = index;
        context.transient->flashUntil = ImGui::GetTime() + 0.12;
    }
    if (selected && context.scrollRequested) {
        ImGui::SetScrollHereY(0.35f);
        context.scrollRequested = false;
    } 

    ImGui::PopID();
    return result;
}

ImGuiWindowFlags ToImGuiFlags(WindowFlags flags) {
    ImGuiWindowFlags result = 0;
    if (flags & Flag(WindowFlag::NoTitleBar)) result |= ImGuiWindowFlags_NoTitleBar;
    if (flags & Flag(WindowFlag::NoResize)) result |= ImGuiWindowFlags_NoResize;
    if (flags & Flag(WindowFlag::NoMove)) result |= ImGuiWindowFlags_NoMove;
    if (flags & Flag(WindowFlag::NoCollapse)) result |= ImGuiWindowFlags_NoCollapse;
    if (flags & Flag(WindowFlag::NoScrollbar)) result |= ImGuiWindowFlags_NoScrollbar;
    if (flags & Flag(WindowFlag::NoBackground)) result |= ImGuiWindowFlags_NoBackground;
    if (flags & Flag(WindowFlag::AlwaysAutoResize)) result |= ImGuiWindowFlags_AlwaysAutoResize;
    if (flags & Flag(WindowFlag::NoSavedSettings)) result |= ImGuiWindowFlags_NoSavedSettings;
    if (flags & Flag(WindowFlag::NoFocusOnAppearing)) result |= ImGuiWindowFlags_NoFocusOnAppearing;
    if (flags & Flag(WindowFlag::NoNavigation)) result |= ImGuiWindowFlags_NoNav;
    return result;
}

ImU32 ToImGuiColor(Color color) {
    return IM_COL32(color.r, color.g, color.b, color.a);
}

int ToImGuiMouseButton(MouseButton button) {
    switch (button) {
    case MouseButton::Right: return ImGuiMouseButton_Right;
    case MouseButton::Middle: return ImGuiMouseButton_Middle;
    case MouseButton::Left:
    default: return ImGuiMouseButton_Left;
    }
}

WindowEntry* FindWindow(const std::string& name) {
    for (auto& window : g_windows) {
        if (window.name == name) return &window;
    }
    return nullptr;
}

void DrawWindows() {
    for (auto& window : g_windows) {
        if (!window.open) continue;
        bool open = window.open;
        if (ImGui::Begin(window.name.c_str(), &open, ToImGuiFlags(window.flags)) && window.drawFn) {
            window.drawFn();
        }
        ImGui::End();
        window.open = open;
    }
}

} // namespace

void Init(const std::string& title) {
    if (g_initialized) return;
    g_title = title;
    g_drawCallbackId = Hooks::RegisterDrawCallback(DrawWindows);
    g_initialized = static_cast<bool>(g_drawCallbackId);
    if (!g_initialized) {
        Log::Error("UI: draw callback registration failed");
        return;
    }
    Log::Info("UI: initialized: " + g_title);
}

void Process() {
    // UI declarations are executed only inside the render hook.
}

void Shutdown() {
    if (g_drawCallbackId) {
        Hooks::UnregisterDrawCallback(g_drawCallbackId);
        g_drawCallbackId = {};
    }
    g_windows.clear();
    g_tabItems.clear();
    g_menuSurfaceStates.clear();
    g_initialized = false;
}

void AddWindow(const std::string& name, DrawFn drawFn, bool defaultOpen, WindowFlags flags) {
    if (FindWindow(name)) return;
    g_windows.push_back({name, std::move(drawFn), defaultOpen, flags});
}

void RemoveWindow(const std::string& name) {
    for (auto it = g_windows.begin(); it != g_windows.end(); ++it) {
        if (it->name == name) {
            g_windows.erase(it);
            return;
        }
    }
}

void SetWindowVisible(const std::string& name, bool visible) {
    if (WindowEntry* window = FindWindow(name)) window->open = visible;
}

bool IsWindowVisible(const std::string& name) {
    const WindowEntry* window = FindWindow(name);
    return window && window->open;
}

void ToggleWindow(const std::string& name) {
    if (WindowEntry* window = FindWindow(name)) window->open = !window->open;
}

void SetNextWindowPosition(Vec2 position, bool always) {
    ImGui::SetNextWindowPos(ImVec2(position.x, position.y), always ? ImGuiCond_Always : ImGuiCond_Once);
}

void SetNextWindowSize(Vec2 size, bool firstUseOnly) {
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y), firstUseOnly ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
}

void SetNextWindowBackgroundAlpha(float alpha) {
    ImGui::SetNextWindowBgAlpha(alpha);
}

void Window(const char* id, const char* title, const DrawFn& drawFn, bool* open, WindowFlags flags) {
    const char* visibleTitle = title ? title : id;
    if (!visibleTitle) return;
    if (ImGui::Begin(visibleTitle, open, ToImGuiFlags(flags)) && drawFn) drawFn();
    ImGui::End();
}

void Child(const char* id, const DrawFn& drawFn, Vec2 size, bool border) {
    if (ImGui::BeginChild(id ? id : "##child", ImVec2(size.x, size.y), border) && drawFn) drawFn();
    ImGui::EndChild();
}

void Disabled(bool disabled, const DrawFn& drawFn) {
    ImGui::BeginDisabled(disabled);
    if (drawFn) drawFn();
    ImGui::EndDisabled();
}

void Indented(const DrawFn& drawFn, float width) {
    ImGui::Indent(width);
    if (drawFn) drawFn();
    ImGui::Unindent(width);
}

void Group(const char* label, const DrawFn& drawFn, Vec2 size) {
    ImGui::BeginChild(label ? label : "##group", ImVec2(size.x, size.y), true, ImGuiWindowFlags_NoScrollbar);
    if (label && label[0]) ImGui::SeparatorText(label);
    if (drawFn) drawFn();
    ImGui::EndChild();
}

void Tree(const char* label, const DrawFn& drawFn, bool defaultOpen) {
    const ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    if (!ImGui::TreeNodeEx(label ? label : "##tree", flags)) return;
    if (drawFn) drawFn();
    ImGui::TreePop();
}

void OpenModal(const char* id) {
    if (id && id[0]) ImGui::OpenPopup(id);
}

void Modal(const char* id, const DrawFn& drawFn, Vec2 size, bool autoResize) {
    if (size.x > 0.0f || size.y > 0.0f) {
        ImGui::SetNextWindowSize(ImVec2(size.x, size.y), ImGuiCond_Appearing);
    }
    const ImGuiWindowFlags flags = autoResize ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_NoResize;
    if (!ImGui::BeginPopupModal(id ? id : "##modal", nullptr, flags)) return;
    if (drawFn) drawFn();
    ImGui::EndPopup();
}

void CloseModal() {
    ImGui::CloseCurrentPopup();
}

void Table(const char* id, const TableColumn* columns, std::size_t columnCount, const DrawFn& drawFn, Vec2 size) {
    if (!columns || columnCount == 0) return;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_SizingStretchProp
        | (size.y > 0.0f ? ImGuiTableFlags_ScrollY : 0);
    if (!ImGui::BeginTable(id ? id : "##table", static_cast<int>(columnCount), flags,
        ImVec2(size.x, size.y))) return;
    for (std::size_t i = 0; i < columnCount; ++i) {
        ImGui::TableSetupColumn(columns[i].label ? columns[i].label : "", ImGuiTableColumnFlags_WidthStretch, columns[i].weight);
    }
    ImGui::TableHeadersRow();
    if (drawFn) drawFn();
    ImGui::EndTable();
}

void TableNextRow() { ImGui::TableNextRow(); }
void TableNextCell() { ImGui::TableNextColumn(); }

MenuSurfaceResult MenuSurface(
    const char* id,
    const char* title,
    const char* subtitle,
    MenuSurfaceState& state,
    bool allowMouse,
    bool showBackHint,
    const DrawFn& drawFn) {
    MenuSurfaceResult result{};
    if (g_menuSurface) return result;

    ClampMenuSurfaceSelection(state);
    const std::string surfaceId = id ? id : "menu-surface";
    MenuSurfaceContext context{};
    context.state = &state;
    context.transient = &g_menuSurfaceStates[surfaceId];
    context.allowMouse = allowMouse;

    if (state.itemCount > 0) {
        if (MenuSurfacePressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp, ImGuiKey_Keypad8, true)) {
            MoveMenuSurfaceSelection(context, -1);
        } else if (MenuSurfacePressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown, ImGuiKey_Keypad2, true)) {
            MoveMenuSurfaceSelection(context, 1);
        }
        if (allowMouse) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel > 0.0f) MoveMenuSurfaceSelection(context, -1);
            if (wheel < 0.0f) MoveMenuSurfaceSelection(context, 1);
        }
    }

    context.backRequested = ImGui::IsKeyPressed(ImGuiKey_Backspace, false)
        || ImGui::IsKeyPressed(ImGuiKey_Escape, false)
        || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)
        || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)
        || (allowMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Right));

    ImGui::SetNextWindowPos(ImVec2(40.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kMenuSurfaceWidth, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(kMenuSurfaceWidth, 320.0f), ImVec2(kMenuSurfaceWidth, 900.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    std::string windowName = title ? title : "XBase";
    windowName += "###";
    windowName += id ? id : "menu-surface";
    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse;

    const bool visible = ImGui::Begin(windowName.c_str(), nullptr, windowFlags);
    if (visible) {
        const ImVec2 windowPosition = ImGui::GetWindowPos();
        const float windowWidth = ImGui::GetWindowWidth();
        const float windowHeight = ImGui::GetWindowHeight();

        if (allowMouse
            && ImGui::IsMouseHoveringRect(
                windowPosition,
                ImVec2(windowPosition.x + windowWidth, windowPosition.y + kMenuSurfaceHeaderHeight))
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(windowPosition.x + delta.x, windowPosition.y + delta.y));
        }

        const ImU32 accent = ToImGuiColor(Theme::GetColors().primary);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            windowPosition,
            ImVec2(windowPosition.x + windowWidth, windowPosition.y + kMenuSurfaceHeaderHeight),
            IM_COL32(0, 0, 0, 245));
        drawList->AddRectFilled(
            ImVec2(windowPosition.x, windowPosition.y + kMenuSurfaceHeaderHeight - 3.0f),
            ImVec2(windowPosition.x + windowWidth, windowPosition.y + kMenuSurfaceHeaderHeight),
            accent);
        drawList->AddText(
            ImVec2(windowPosition.x + kMenuSurfacePaddingX, windowPosition.y + 10.0f),
            IM_COL32(255, 255, 255, 255),
            title ? title : "XBase");
        if (subtitle && subtitle[0]) {
            drawList->AddText(
                ImVec2(
                    windowPosition.x + kMenuSurfacePaddingX,
                    windowPosition.y + 12.0f + ImGui::GetTextLineHeight()),
                IM_COL32(255, 255, 255, 150),
                subtitle);
        }

        ImGui::SetCursorPos(ImVec2(0.0f, kMenuSurfaceHeaderHeight));
        const float contentHeight = windowHeight - kMenuSurfaceHeaderHeight - kMenuSurfaceFooterHeight;
        if (ImGui::BeginChild(
            "##menu-surface-body",
            ImVec2(windowWidth, contentHeight),
            false,
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            const ImVec2 bodyMin = ImGui::GetWindowPos();
            const ImVec2 bodyMax(bodyMin.x + ImGui::GetWindowWidth(), bodyMin.y + ImGui::GetWindowHeight());
            ImGui::GetWindowDrawList()->AddRectFilled(bodyMin, bodyMax, IM_COL32(10, 10, 12, 180));
            g_menuSurface = &context;
            if (drawFn) drawFn();
            g_menuSurface = nullptr;
        }
        ImGui::EndChild();

        state.itemCount = context.currentIndex;
        ClampMenuSurfaceSelection(state);

        char pageText[32];
        std::snprintf(
            pageText,
            sizeof(pageText),
            "%d / %d",
            state.itemCount > 0 ? state.selectedIndex + 1 : 0,
            state.itemCount);
        const ImVec2 pageSize = ImGui::CalcTextSize(pageText);
        drawList->AddRectFilled(
            ImVec2(windowPosition.x + windowWidth - kMenuSurfacePaddingX - pageSize.x - 6.0f, windowPosition.y + 10.0f),
            ImVec2(windowPosition.x + windowWidth - 4.0f, windowPosition.y + 16.0f + pageSize.y),
            IM_COL32(0, 0, 0, 245));
        drawList->AddText(
            ImVec2(windowPosition.x + windowWidth - kMenuSurfacePaddingX - pageSize.x, windowPosition.y + 14.0f),
            IM_COL32(255, 255, 255, 180),
            pageText);

        const float footerTop = windowPosition.y + windowHeight - kMenuSurfaceFooterHeight;
        drawList->AddRectFilled(
            ImVec2(windowPosition.x, footerTop),
            ImVec2(windowPosition.x + windowWidth, windowPosition.y + windowHeight),
            IM_COL32(0, 0, 0, 240));
        drawList->AddRectFilled(
            ImVec2(windowPosition.x, footerTop),
            ImVec2(windowPosition.x + windowWidth, footerTop + 2.0f),
            accent);

        const char* hint = nullptr;
        if (allowMouse) {
            hint = context.selectionCanAdjust
                ? "LMB confirm/adjust   Wheel move   RMB back"
                : "LMB select/confirm   Wheel move   RMB back";
        } else {
            hint = context.selectionCanAdjust
                ? "Enter confirm  Left/Right adjust  Back return"
                : "Enter confirm  Up/Down select  Back return";
        }
        if (!showBackHint) {
            hint = context.selectionCanAdjust ? "Enter confirm  Left/Right adjust" : "Enter confirm  Up/Down select";
        }
        drawList->AddText(
            ImVec2(
                windowPosition.x + kMenuSurfacePaddingX,
                footerTop + (kMenuSurfaceFooterHeight - ImGui::GetTextLineHeight()) * 0.5f),
            IM_COL32(255, 255, 255, 140),
            hint);
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(5);
    g_menuSurface = nullptr;

    result.backRequested = context.backRequested;
    result.selectionCanAdjust = context.selectionCanAdjust;
    return result;
}

bool MenuSurfaceButton(const char* label) {
    return DrawMenuSurfaceRow(label, "", false).activated;
}

bool MenuSurfaceCheckbox(const char* label, bool& value) {
    const MenuSurfaceRowResult row = DrawMenuSurfaceRow(label, value ? "ON" : "OFF", false);
    if (!row.activated) return false;
    value = !value;
    return true;
}

bool MenuSurfaceCollapsingSection(const char* label, bool& open) {
    const MenuSurfaceRowResult row = DrawMenuSurfaceRow(label, open ? "v" : ">", false);
    if (row.activated) open = !open;
    return open;
}

bool MenuSurfaceSlider(const char* label, float& value, float minValue, float maxValue, const char* format) {
    char valueText[32];
    std::snprintf(valueText, sizeof(valueText), format ? format : "%.1f", value);
    char rightText[48];
    std::snprintf(rightText, sizeof(rightText), "< %s >", valueText);
    const MenuSurfaceRowResult row = DrawMenuSurfaceRow(label, rightText, true);
    const float step = maxValue - minValue > 100.0f ? 1.0f : 0.1f;
    const float previous = value;
    if (row.stepLeft) value -= step;
    if (row.stepRight) value += step;
    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;
    return value != previous;
}

bool MenuSurfaceSlider(const char* label, int& value, int minValue, int maxValue) {
    char rightText[32];
    std::snprintf(rightText, sizeof(rightText), "< %d >", value);
    const MenuSurfaceRowResult row = DrawMenuSurfaceRow(label, rightText, true);
    const int previous = value;
    if (row.stepLeft) --value;
    if (row.stepRight) ++value;
    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;
    return value != previous;
}

bool MenuSurfaceInput(const char* label, float& value, float step, float fastStep, const char* format) {
    (void)step;
    (void)fastStep;
    return MenuSurfaceSlider(label, value, -10000.0f, 10000.0f, format);
}

bool MenuSurfaceInput(const char* label, int& value, int step, int fastStep) {
    (void)fastStep;
    char rightText[32];
    std::snprintf(rightText, sizeof(rightText), "< %d >", value);
    const MenuSurfaceRowResult row = DrawMenuSurfaceRow(label, rightText, true);
    const int previous = value;
    if (row.stepLeft) value -= step;
    if (row.stepRight) value += step;
    return value != previous;
}

void MenuSurfaceSection(const char* label) {
    if (!g_menuSurface) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    constexpr float height = 26.0f;
    drawList->AddRectFilled(position, ImVec2(position.x + width, position.y + height), IM_COL32(0, 0, 0, 90));
    drawList->AddText(
        ImVec2(position.x + kMenuSurfacePaddingX, position.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
        ToImGuiColor(Theme::GetColors().primary),
        label ? label : "");
    ImGui::Dummy(ImVec2(width, height));
}

void Tabs(const char* id, const DrawFn& drawFn) {
    if (g_menuSurface) {
        if (drawFn) drawFn();
        return;
    }
    const bool previous = g_tabsOpen;
    g_tabsOpen = ImGui::BeginTabBar(
        id ? id : "##tabs",
        ImGuiTabBarFlags_NoTooltip | ImGuiTabBarFlags_FittingPolicyScroll);
    if (g_tabsOpen && drawFn) drawFn();
    if (g_tabsOpen) ImGui::EndTabBar();
    g_tabsOpen = previous;
}

void Tab(const char* id, const char* label, const DrawFn& drawFn) {
    if (g_menuSurface) {
        MenuSurfaceSection(label);
        ImGui::PushID(id ? id : label);
        if (drawFn) drawFn();
        ImGui::PopID();
        return;
    }
    if (!g_tabsOpen || !ImGui::BeginTabItem(label ? label : "")) return;
    if (drawFn) drawFn();
    ImGui::EndTabItem();
}

Vec2 GridItemSize(int columns, bool includeSpacing) {
    if (columns <= 1) return {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.3f};
    const float rowWidth = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetCursorStartPos().x;
    const float spacing = includeSpacing ? ImGui::GetStyle().ItemSpacing.x * (columns - 1) : 0.0f;
    return {(rowWidth - spacing) / columns, ImGui::GetFrameHeight() * 1.3f};
}

bool CollapsingSection(const char* label, bool& open) {
    ImGui::SetNextItemOpen(open, ImGuiCond_Always);
    ImGui::CollapsingHeader(label ? label : "");
    if (ImGui::IsItemClicked()) open = !open;
    return open;
}

void Text(const char* text) { ImGui::TextUnformatted(text ? text : ""); }
void Text(const char* format, float value) { ImGui::Text(format ? format : "", value); }
void Text(const char* format, int value) { ImGui::Text(format ? format : "", value); }
void Text(const char* format, int first, int second) { ImGui::Text(format ? format : "", first, second); }
void Text(const char* format, const char* value) {
    ImGui::Text(format ? format : "", value ? value : "");
}
void Text(const char* format, const char* first, const char* second) {
    ImGui::Text(format ? format : "", first ? first : "", second ? second : "");
}
void Text(const char* format, float first, float second, float third) {
    ImGui::Text(format ? format : "", first, second, third);
}
void Text(const char* format, float first, float second, int third, int fourth) {
    ImGui::Text(format ? format : "", first, second, third, fourth);
}
void TextWrapped(const char* text) { ImGui::TextWrapped("%s", text ? text : ""); }
void TextWrapped(const char* format, const char* value) {
    ImGui::TextWrapped(format ? format : "", value ? value : "");
}
void TextWrapped(const char* format, const char* first, const char* second) {
    ImGui::TextWrapped(format ? format : "", first ? first : "", second ? second : "");
}
void TextDisabled(const char* text) { ImGui::TextDisabled("%s", text ? text : ""); }
void TextDisabled(const char* format, int value) { ImGui::TextDisabled(format ? format : "", value); }
void TextDisabled(const char* format, const char* value) {
    ImGui::TextDisabled(format ? format : "", value ? value : "");
}
void TextDisabled(const char* format, const char* first, const char* second) {
    ImGui::TextDisabled(format ? format : "", first ? first : "", second ? second : "");
}
void TextDisabled(const char* format, int first, int second, int third) {
    ImGui::TextDisabled(format ? format : "", first, second, third);
}
void Spacing() { ImGui::Spacing(); }
float GetFrameRate() { return ImGui::GetIO().Framerate; }

void CenterText(const char* text) {
    if (!text) return;
    const float width = ImGui::CalcTextSize(text).x;
    const float available = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available - width) * 0.5f);
    ImGui::TextUnformatted(text);
}

void Separator() { ImGui::Separator(); }
void SeparatorText(const char* label) { ImGui::SeparatorText(label ? label : ""); }

void Tooltip(const char* text) {
    if (!text || !ImGui::IsItemHovered()) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void HelpMarker(const char* desc, bool* hold) {
    if (!desc) return;
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (hold) {
        *hold = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool Button(const char* label, Vec2 size) { return ImGui::Button(label, ImVec2(size.x, size.y)); }

bool StyledButton(const char* label, Vec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Theme::GetColors().primary));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Theme::GetColors().primary));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Theme::GetColors().accent));
    bool result = ImGui::Button(label, ImVec2(size.x, size.y));
    ImGui::PopStyleColor(3);
    return result;
}

void BeginGroupBox(const char* label, Vec2 size) {
    ImGui::BeginGroup();
    ImGui::BeginChild(label, ImVec2(size.x, size.y), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_NoBackground);
}

void EndGroupBox() {
    ImGui::EndChild();
    ImGui::SameLine();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    cursorPos.y += ImGui::GetFontSize();
    ImGui::SetCursorScreenPos(cursorPos);
    ImGui::Separator();
    ImGui::EndGroup();
}

bool Checkbox(const char* label, bool& value) { return ImGui::Checkbox(label, &value); }
bool Choice(const char* label, int& selectedValue, int value) {
    return ImGui::RadioButton(label ? label : "", &selectedValue, value);
}
bool Slider(const char* label, float& value, float minValue, float maxValue, const char* format) {
    return ImGui::SliderFloat(label, &value, minValue, maxValue, format);
}
bool Slider(const char* label, int& value, int minValue, int maxValue) {
    return ImGui::SliderInt(label, &value, minValue, maxValue);
}
bool Input(const char* label, float& value, float step, float fastStep, const char* format) {
    return ImGui::InputFloat(label, &value, step, fastStep, format);
}
bool Input(const char* label, int& value, int step, int fastStep) {
    return ImGui::InputInt(label, &value, step, fastStep);
}
bool InputText(const char* label, char* value, std::size_t capacity, const char* hint, bool readOnly, bool submitOnEnter) {
    if (!value || capacity == 0) return false;
    ImGuiInputTextFlags flags = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
    if (submitOnEnter) flags |= ImGuiInputTextFlags_EnterReturnsTrue;
    if (hint) return ImGui::InputTextWithHint(label, hint, value, capacity, flags);
    return ImGui::InputText(label, value, capacity, flags);
}
bool InputTextMultiline(const char* label, char* value, std::size_t capacity, Vec2 size, bool readOnly) {
    if (!value || capacity == 0) return false;
    const ImGuiInputTextFlags flags = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
    return ImGui::InputTextMultiline(label ? label : "##multiline", value, capacity, ImVec2(size.x, size.y), flags);
}
void SetClipboardText(const char* text) { ImGui::SetClipboardText(text ? text : ""); }
bool Selectable(const char* label, bool selected, Vec2 size) {
    return ImGui::Selectable(label, selected, 0, ImVec2(size.x, size.y));
}
void Combo(const char* label, const char* preview, const DrawFn& drawFn) {
    if (!ImGui::BeginCombo(label ? label : "##combo", preview ? preview : "")) return;
    if (drawFn) drawFn();
    ImGui::EndCombo();
}
void FocusLastItemByDefault() { ImGui::SetItemDefaultFocus(); }
bool MenuItem(const char* label, bool selected, bool enabled) {
    return ImGui::MenuItem(label ? label : "", nullptr, selected, enabled);
}
bool CollapsingHeader(const char* label, bool defaultOpen) {
    return ImGui::CollapsingHeader(label ? label : "", defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}
bool InvisibleButton(const char* id, Vec2 size) {
    return ImGui::InvisibleButton(id ? id : "##invisible", ImVec2(size.x, size.y));
}
bool IsLastItemHovered() { return ImGui::IsItemHovered(); }
bool IsMouseDown(MouseButton button) { return ImGui::IsMouseDown(ToImGuiMouseButton(button)); }
Vec2 GetMousePosition() {
    const ImVec2 value = ImGui::GetIO().MousePos;
    return {value.x, value.y};
}
Vec2 GetCursorScreenPosition() {
    const ImVec2 value = ImGui::GetCursorScreenPos();
    return {value.x, value.y};
}
Vec2 GetContentAvailable() {
    const ImVec2 value = ImGui::GetContentRegionAvail();
    return {value.x, value.y};
}

namespace Canvas {
void Line(Vec2 from, Vec2 to, Color color, float thickness) {
    ImGui::GetWindowDrawList()->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), ToImGuiColor(color), thickness);
}
void Rect(Vec2 min, Vec2 max, Color color, float thickness) {
    ImGui::GetWindowDrawList()->AddRect(ImVec2(min.x, min.y), ImVec2(max.x, max.y), ToImGuiColor(color), 0.0f, 0, thickness);
}
void RectFilled(Vec2 min, Vec2 max, Color color) {
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, max.y), ToImGuiColor(color));
}
void Circle(Vec2 center, float radius, Color color, float thickness) {
    ImGui::GetWindowDrawList()->AddCircle(ImVec2(center.x, center.y), radius, ToImGuiColor(color), 0, thickness);
}
void CircleFilled(Vec2 center, float radius, Color color) {
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(center.x, center.y), radius, ToImGuiColor(color));
}
void Text(Vec2 position, Color color, const char* text) {
    ImGui::GetWindowDrawList()->AddText(ImVec2(position.x, position.y), ToImGuiColor(color), text ? text : "");
}

void Arc(Vec2 center, float radius, float startAngle, float endAngle, Color color, float thickness) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 imColor = ToImGuiColor(color);
    int numSegments = static_cast<int>((endAngle - startAngle) * radius * 0.5f);
    if (numSegments < 12) numSegments = 12;
    if (numSegments > 64) numSegments = 64;
    float angleStep = (endAngle - startAngle) / numSegments;
    float angle = startAngle;
    Vec2 prev = {center.x + cosf(angle) * radius, center.y + sinf(angle) * radius};
    for (int i = 0; i < numSegments; ++i) {
        angle += angleStep;
        Vec2 curr = {center.x + cosf(angle) * radius, center.y + sinf(angle) * radius};
        drawList->AddLine(ImVec2(prev.x, prev.y), ImVec2(curr.x, curr.y), imColor, thickness);
        prev = curr;
    }
}

void Polyline(const Vec2* points, std::size_t count, Color color, float thickness) {
    if (!points || count < 2) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 imColor = ToImGuiColor(color);
    std::vector<ImVec2> verts;
    verts.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        verts.push_back(ImVec2(points[i].x, points[i].y));
    }
    drawList->AddPolyline(verts.data(), static_cast<int>(count), imColor, 0, thickness);
}
} // namespace Canvas

void SameLine() { ImGui::SameLine(); }
void Columns(int count, const char* id, bool border) { ImGui::Columns(count, id, border); }
void NextColumn() { ImGui::NextColumn(); }
void PushItemWidth(float width) { ImGui::PushItemWidth(width); }
void PopItemWidth() { ImGui::PopItemWidth(); }

void BeginTabBar(const std::string& name) {
    g_tabBarName = name;
    g_tabItems.clear();
    g_activeTab = 0;
}

void AddTab(const std::string& label, DrawFn drawFn) {
    g_tabItems.push_back({label, std::move(drawFn)});
}

bool RenderTabBar(float height) {
    if (g_tabItems.empty()) return false;
    if (height > 0.0f) ImGui::BeginChild("##tab-body", ImVec2(0.0f, height), true);

    bool changed = false;
    if (ImGui::BeginTabBar(g_tabBarName.c_str())) {
        for (int index = 0; index < static_cast<int>(g_tabItems.size()); ++index) {
            if (ImGui::BeginTabItem(g_tabItems[index].label.c_str())) {
                changed = changed || g_activeTab != index;
                g_activeTab = index;
                if (g_tabItems[index].drawFn) g_tabItems[index].drawFn();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    if (height > 0.0f) ImGui::EndChild();
    return changed;
}

void EndTabBar() {
    g_tabItems.clear();
}

struct ActiveNotification {
    std::string message;
    float duration;
    float elapsed;
    Color color;
};

static std::list<ActiveNotification> s_notifications;

void Notify(NotificationSpec spec) {
    if (!spec.message || spec.message[0] == '\0') return;
    s_notifications.push_back({spec.message, spec.duration, 0.0f, spec.color});
}

void RenderNotifications(Vec2 screenPosition) {
    if (s_notifications.empty()) return;
    float frameRate = GetFrameRate();
    float dt = 1.0f / (frameRate > 0.0f ? frameRate : 60.0f);

    for (auto it = s_notifications.begin(); it != s_notifications.end();) {
        it->elapsed += dt;
        if (it->elapsed >= it->duration) {
            it = s_notifications.erase(it);
        } else {
            ++it;
        }
    }
    if (s_notifications.empty()) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    Vec2 basePos = screenPosition;

    float posY = basePos.y;
    float toastHeight = 36.0f;
    float padding = 8.0f;
    float margin = 10.0f;

    for (auto it = s_notifications.rbegin(); it != s_notifications.rend(); ++it) {
        float progress = 1.0f - (it->elapsed / it->duration);
        float alpha = ImSaturate(progress * 1.5f);
        if (alpha < 0.05f) alpha = 0.05f;

        Color toastColor = it->color;
        ColorF toastCf = ToColorF(toastColor);
        ColorF bgCf(0.10f, 0.10f, 0.12f, 0.92f * alpha);
        ColorF textCf(1.0f, 1.0f, 1.0f, 1.0f * alpha);

        Vec2 size = {320.0f, toastHeight};
        Vec2 min = {basePos.x + margin, posY + margin};
        Vec2 max = {min.x + size.x, min.y + size.y};

        drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, max.y),
                                ToImGuiColor(bgCf), 6.0f, 0, 0.0f);
        drawList->AddRect(ImVec2(min.x, min.y), ImVec2(max.x, max.y),
                          ToImGuiColor(toastCf), 6.0f, 0, 1.0f * alpha);

        std::string label = std::string(it->message);
        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        drawList->AddText(ImVec2(min.x + padding, min.y + (size.y - textSize.y) * 0.5f),
                          ToImGuiColor(textCf), label.c_str());

        float barWidth = size.x * progress;
        if (barWidth > 0.0f) {
            drawList->AddRectFilled(ImVec2(min.x, max.y),
                                    ImVec2(min.x + barWidth, max.y + 3.0f),
                                    ToImGuiColor(toastCf), 0.0f, 0, 1.0f * alpha);
        }

        posY += size.y + padding;
    }
}

} // namespace XBase::UI