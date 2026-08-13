#include <XBase/Theme.h>
#include <XBase/Config.h>
#include <XBase/Log.h>

#include "imgui/imgui.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace XBase::Theme {

namespace {

ColorSet s_custom;
std::vector<ImFont*> s_fonts;
ImGuiContext* s_context = nullptr;
std::uint16_t s_contextGeneration = 0;

constexpr std::uint32_t FontIndexMask = 0xFFFFu;

bool HasActiveContext() {
    return s_context && ImGui::GetCurrentContext() == s_context;
}

std::uint16_t NextGeneration(std::uint16_t generation) {
    ++generation;
    return generation == 0 ? 1 : generation;
}

FontId MakeFontId(std::size_t index) {
    if (index == 0 || index > FontIndexMask) return {};
    return FontId{
        (static_cast<std::uint32_t>(s_contextGeneration) << 16)
        | static_cast<std::uint32_t>(index)
    };
}

bool ResolveFont(FontId font, ImFont*& resolved) {
    if (!HasActiveContext() || !font) return false;
    const auto generation = static_cast<std::uint16_t>(font.value >> 16);
    const auto index = static_cast<std::uint16_t>(font.value & FontIndexMask);
    if (generation != s_contextGeneration || index == 0 || index > s_fonts.size()) return false;
    resolved = s_fonts[index - 1];
    return resolved != nullptr;
}

ImVec4 ToImVec4(ColorF color) {
    return {color.r, color.g, color.b, color.a};
}

ColorF ToColorF(const ImVec4& color) {
    return {color.x, color.y, color.z, color.w};
}

} // namespace

void ApplyStyle(const Style& theme) {
    if (!HasActiveContext()) return;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = theme.windowRounding;
    style.WindowPadding = ImVec2(theme.windowPadding.x, theme.windowPadding.y);
    style.ItemSpacing = ImVec2(theme.itemSpacing.x, theme.itemSpacing.y);
    style.FrameRounding = theme.frameRounding;
    style.TabRounding = theme.tabRounding;
    style.ChildRounding = theme.frameRounding;
    style.PopupRounding = theme.frameRounding;
    style.ScrollbarRounding = theme.frameRounding;
    style.GrabRounding = theme.frameRounding;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ToImVec4(theme.text);
    colors[ImGuiCol_TextDisabled] = ToImVec4(theme.textDisabled);
    colors[ImGuiCol_WindowBg] = ToImVec4(theme.windowBackground);
    colors[ImGuiCol_ChildBg] = ToImVec4(theme.childBackground);
    colors[ImGuiCol_PopupBg] = ToImVec4(theme.popupBackground);
    colors[ImGuiCol_Border] = ToImVec4(theme.border);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = ToImVec4(theme.frameBackground);
    colors[ImGuiCol_FrameBgHovered] = ToImVec4(theme.frameBackgroundHovered);
    colors[ImGuiCol_FrameBgActive] = ToImVec4(theme.frameBackgroundActive);
    colors[ImGuiCol_TitleBg] = ToImVec4(theme.titleBackground);
    colors[ImGuiCol_TitleBgActive] = ToImVec4(theme.titleBackgroundActive);
    colors[ImGuiCol_TitleBgCollapsed] = ToImVec4(theme.titleBackground);
    colors[ImGuiCol_MenuBarBg] = ToImVec4(theme.titleBackground);
    colors[ImGuiCol_ScrollbarBg] = ToImVec4(theme.scrollbarBackground);
    colors[ImGuiCol_ScrollbarGrab] = ToImVec4(theme.scrollbarGrab);
    colors[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(theme.headerHovered);
    colors[ImGuiCol_ScrollbarGrabActive] = ToImVec4(theme.headerActive);
    colors[ImGuiCol_CheckMark] = ToImVec4(theme.checkMark);
    colors[ImGuiCol_SliderGrab] = ToImVec4(theme.sliderGrab);
    colors[ImGuiCol_SliderGrabActive] = ToImVec4(theme.sliderGrabActive);
    colors[ImGuiCol_Button] = ToImVec4(theme.button);
    colors[ImGuiCol_ButtonHovered] = ToImVec4(theme.buttonHovered);
    colors[ImGuiCol_ButtonActive] = ToImVec4(theme.buttonActive);
    colors[ImGuiCol_Header] = ToImVec4(theme.header);
    colors[ImGuiCol_HeaderHovered] = ToImVec4(theme.headerHovered);
    colors[ImGuiCol_HeaderActive] = ToImVec4(theme.headerActive);
    colors[ImGuiCol_Separator] = ToImVec4(theme.separator);
    colors[ImGuiCol_SeparatorHovered] = ToImVec4(theme.headerHovered);
    colors[ImGuiCol_SeparatorActive] = ToImVec4(theme.headerActive);
    colors[ImGuiCol_ResizeGrip] = ToImVec4(theme.header);
    colors[ImGuiCol_ResizeGripHovered] = ToImVec4(theme.headerHovered);
    colors[ImGuiCol_ResizeGripActive] = ToImVec4(theme.headerActive);
    colors[ImGuiCol_Tab] = ToImVec4(theme.button);
    colors[ImGuiCol_TabHovered] = ToImVec4(theme.tabHovered);
    colors[ImGuiCol_TabActive] = ToImVec4(theme.tabActive);
    colors[ImGuiCol_TabUnfocused] = ToImVec4(theme.frameBackground);
    colors[ImGuiCol_TabUnfocusedActive] = ToImVec4(theme.header);
    colors[ImGuiCol_PlotLines] = ToImVec4(theme.checkMark);
    colors[ImGuiCol_PlotLinesHovered] = ToImVec4(theme.sliderGrabActive);
    colors[ImGuiCol_PlotHistogram] = ToImVec4(theme.checkMark);
    colors[ImGuiCol_PlotHistogramHovered] = ToImVec4(theme.sliderGrabActive);
    colors[ImGuiCol_TableHeaderBg] = ToImVec4(theme.header);
    colors[ImGuiCol_TableBorderStrong] = ToImVec4(theme.border);
    colors[ImGuiCol_TableBorderLight] = ToImVec4(theme.separator);
    colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(theme.text.r, theme.text.g, theme.text.b, 0.04f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(theme.headerActive.r, theme.headerActive.g, theme.headerActive.b, 0.40f);
    colors[ImGuiCol_DragDropTarget] = ToImVec4(theme.checkMark);
    colors[ImGuiCol_NavHighlight] = ToImVec4(theme.navigationHighlight);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.45f);
}

void ConfigureInteraction(bool keyboardNavigation, bool mouseEnabled) {
    if (!HasActiveContext()) return;
    ImGuiIO& io = ImGui::GetIO();
    if (keyboardNavigation) io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    else io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

    if (mouseEnabled) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void ApplyPreset(Preset preset) {
    if (!HasActiveContext()) return;
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;

    switch (preset) {
        case Preset::Dark:
            style.Colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
            style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
            style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
            style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
            style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.10f, 0.12f, 0.94f);
            style.Colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
            style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
            style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
            style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
            style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
            style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.32f, 0.32f, 0.36f, 1.00f);
            style.Colors[ImGuiCol_Header]                = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.32f, 0.32f, 0.36f, 1.00f);
            style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.38f, 0.38f, 0.42f, 1.00f);
            style.Colors[ImGuiCol_Separator]             = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
            style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.38f, 0.38f, 0.42f, 1.00f);
            style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
            style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
            style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
            style.Colors[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_TabActive]             = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
            break;

        case Preset::Light:
            style.Colors[ImGuiCol_Text]                  = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
            style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.95f, 0.95f, 0.96f, 0.94f);
            style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
            style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.94f, 0.94f, 0.95f, 0.94f);
            style.Colors[ImGuiCol_Border]                = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.84f, 0.84f, 0.87f, 1.00f);
            style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.78f, 0.78f, 0.82f, 1.00f);
            style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.87f, 0.87f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_Button]                = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.82f, 0.82f, 0.85f, 1.00f);
            style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.76f, 0.76f, 0.80f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
            style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_Header]                = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
            style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.74f, 0.74f, 0.78f, 1.00f);
            style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.68f, 0.68f, 0.72f, 1.00f);
            style.Colors[ImGuiCol_Separator]             = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
            style.Colors[ImGuiCol_Tab]                   = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
            style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.84f, 0.84f, 0.87f, 1.00f);
            style.Colors[ImGuiCol_TabActive]             = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.94f, 0.94f, 0.95f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
            break;

        case Preset::ClassicBlue:
            style.Colors[ImGuiCol_Text]                  = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
            style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.10f, 0.16f, 0.94f);
            style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.08f, 0.12f, 0.18f, 1.00f);
            style.Colors[ImGuiCol_Border]                = ImVec4(0.14f, 0.22f, 0.35f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.10f, 0.14f, 0.22f, 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.14f, 0.20f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.18f, 0.26f, 0.38f, 1.00f);
            style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.12f, 0.18f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
            style.Colors[ImGuiCol_Button]                = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
            style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.18f, 0.26f, 0.38f, 1.00f);
            style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.22f, 0.34f, 0.48f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.28f, 0.56f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 0.90f, 1.00f);
            style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.36f, 0.64f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_Header]                = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
            style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.16f, 0.24f, 0.36f, 1.00f);
            style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.20f, 0.30f, 0.44f, 1.00f);
            style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.12f, 0.18f, 1.00f);
            style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.14f, 0.20f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_TabActive]             = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
            break;

        default: break;
    }

    s_custom = GetColors();
}

void Init() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context) return;
    if (s_context != context) {
        s_context = context;
        s_contextGeneration = NextGeneration(s_contextGeneration);
        s_fonts.clear();
    }
    ApplyPreset(Preset::Dark);
}

void Shutdown() {
    s_fonts.clear();
    s_context = nullptr;
    s_contextGeneration = NextGeneration(s_contextGeneration);
}

void ApplyCustom(const ColorSet& colors) {
    if (!HasActiveContext()) return;
    s_custom = colors;

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_CheckMark] = ToImVec4(colors.primary);
    style.Colors[ImGuiCol_SliderGrabActive] = ToImVec4(colors.primary);
    style.Colors[ImGuiCol_ButtonActive] = ToImVec4(colors.accent);
    style.Colors[ImGuiCol_WindowBg] = ToImVec4(colors.background);
    style.Colors[ImGuiCol_FrameBg] = ToImVec4(colors.surface);
    style.Colors[ImGuiCol_Text] = ToImVec4(colors.text);
    style.Colors[ImGuiCol_TextDisabled] = ToImVec4(colors.textDisabled);
    style.Colors[ImGuiCol_Border] = ToImVec4(colors.border);
    style.Colors[ImGuiCol_HeaderActive] = ToImVec4(colors.highlight);
}

ColorSet GetColors() {
    if (!HasActiveContext()) return s_custom;
    ImGuiStyle& style = ImGui::GetStyle();
    ColorSet cs;
    cs.primary      = ToColorF(style.Colors[ImGuiCol_CheckMark]);
    cs.accent       = ToColorF(style.Colors[ImGuiCol_ButtonActive]);
    cs.background   = ToColorF(style.Colors[ImGuiCol_WindowBg]);
    cs.surface      = ToColorF(style.Colors[ImGuiCol_FrameBg]);
    cs.text         = ToColorF(style.Colors[ImGuiCol_Text]);
    cs.textDisabled = ToColorF(style.Colors[ImGuiCol_TextDisabled]);
    cs.border       = ToColorF(style.Colors[ImGuiCol_Border]);
    cs.highlight    = ToColorF(style.Colors[ImGuiCol_HeaderActive]);
    return cs;
}

FontId LoadFont(const char* path, float size) {
    if (!path || path[0] == '\0' || size <= 0.0f) {
        Log::Warn("Theme: invalid font request");
        return {};
    }

    if (!HasActiveContext()) {
        Log::Warn("Theme: font request without an active render context");
        return {};
    }

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size);
    if (!font) {
        Log::Warn(("Theme: failed to load font " + std::string(path)).c_str());
        return {};
    }

    s_fonts.push_back(font);
    Log::Info(("Theme: loaded font " + std::string(path)).c_str());
    return MakeFontId(s_fonts.size());
}

bool SetDefaultFont(FontId font) {
    ImFont* resolved = nullptr;
    if (!ResolveFont(font, resolved)) return false;

    ImGui::GetIO().FontDefault = resolved;
    return true;
}

void SavePreset(const std::string& name) {
    if (!HasActiveContext()) {
        Log::Warn("Theme: cannot save preset without active context");
        return;
    }

    ColorSet cs = GetColors();
    std::string prefix = "theme." + name + ".";

    Config::SetFloat(prefix + "primary.r", cs.primary.r);
    Config::SetFloat(prefix + "primary.g", cs.primary.g);
    Config::SetFloat(prefix + "primary.b", cs.primary.b);
    Config::SetFloat(prefix + "primary.a", cs.primary.a);

    Config::SetFloat(prefix + "accent.r", cs.accent.r);
    Config::SetFloat(prefix + "accent.g", cs.accent.g);
    Config::SetFloat(prefix + "accent.b", cs.accent.b);
    Config::SetFloat(prefix + "accent.a", cs.accent.a);

    Config::SetFloat(prefix + "background.r", cs.background.r);
    Config::SetFloat(prefix + "background.g", cs.background.g);
    Config::SetFloat(prefix + "background.b", cs.background.b);
    Config::SetFloat(prefix + "background.a", cs.background.a);

    Config::SetFloat(prefix + "surface.r", cs.surface.r);
    Config::SetFloat(prefix + "surface.g", cs.surface.g);
    Config::SetFloat(prefix + "surface.b", cs.surface.b);
    Config::SetFloat(prefix + "surface.a", cs.surface.a);

    Config::SetFloat(prefix + "text.r", cs.text.r);
    Config::SetFloat(prefix + "text.g", cs.text.g);
    Config::SetFloat(prefix + "text.b", cs.text.b);
    Config::SetFloat(prefix + "text.a", cs.text.a);

    Config::SetFloat(prefix + "textDisabled.r", cs.textDisabled.r);
    Config::SetFloat(prefix + "textDisabled.g", cs.textDisabled.g);
    Config::SetFloat(prefix + "textDisabled.b", cs.textDisabled.b);
    Config::SetFloat(prefix + "textDisabled.a", cs.textDisabled.a);

    Config::SetFloat(prefix + "border.r", cs.border.r);
    Config::SetFloat(prefix + "border.g", cs.border.g);
    Config::SetFloat(prefix + "border.b", cs.border.b);
    Config::SetFloat(prefix + "border.a", cs.border.a);

    Config::SetFloat(prefix + "highlight.r", cs.highlight.r);
    Config::SetFloat(prefix + "highlight.g", cs.highlight.g);
    Config::SetFloat(prefix + "highlight.b", cs.highlight.b);
    Config::SetFloat(prefix + "highlight.a", cs.highlight.a);

    Config::Save();
}

bool LoadPreset(const std::string& name) {
    std::string prefix = "theme." + name + ".";

    if (!Config::HasKey(prefix + "primary.r")) {
        Log::Warn(("Theme: preset '" + name + "' not found in config").c_str());
        return false;
    }

    ColorSet cs;
    cs.primary      = {Config::GetFloat(prefix + "primary.r", cs.primary.r),
                       Config::GetFloat(prefix + "primary.g", cs.primary.g),
                       Config::GetFloat(prefix + "primary.b", cs.primary.b),
                       Config::GetFloat(prefix + "primary.a", cs.primary.a)};
    cs.accent       = {Config::GetFloat(prefix + "accent.r", cs.accent.r),
                       Config::GetFloat(prefix + "accent.g", cs.accent.g),
                       Config::GetFloat(prefix + "accent.b", cs.accent.b),
                       Config::GetFloat(prefix + "accent.a", cs.accent.a)};
    cs.background   = {Config::GetFloat(prefix + "background.r", cs.background.r),
                       Config::GetFloat(prefix + "background.g", cs.background.g),
                       Config::GetFloat(prefix + "background.b", cs.background.b),
                       Config::GetFloat(prefix + "background.a", cs.background.a)};
    cs.surface      = {Config::GetFloat(prefix + "surface.r", cs.surface.r),
                       Config::GetFloat(prefix + "surface.g", cs.surface.g),
                       Config::GetFloat(prefix + "surface.b", cs.surface.b),
                       Config::GetFloat(prefix + "surface.a", cs.surface.a)};
    cs.text         = {Config::GetFloat(prefix + "text.r", cs.text.r),
                       Config::GetFloat(prefix + "text.g", cs.text.g),
                       Config::GetFloat(prefix + "text.b", cs.text.b),
                       Config::GetFloat(prefix + "text.a", cs.text.a)};
    cs.textDisabled = {Config::GetFloat(prefix + "textDisabled.r", cs.textDisabled.r),
                       Config::GetFloat(prefix + "textDisabled.g", cs.textDisabled.g),
                       Config::GetFloat(prefix + "textDisabled.b", cs.textDisabled.b),
                       Config::GetFloat(prefix + "textDisabled.a", cs.textDisabled.a)};
    cs.border       = {Config::GetFloat(prefix + "border.r", cs.border.r),
                       Config::GetFloat(prefix + "border.g", cs.border.g),
                       Config::GetFloat(prefix + "border.b", cs.border.b),
                       Config::GetFloat(prefix + "border.a", cs.border.a)};
    cs.highlight    = {Config::GetFloat(prefix + "highlight.r", cs.highlight.r),
                       Config::GetFloat(prefix + "highlight.g", cs.highlight.g),
                       Config::GetFloat(prefix + "highlight.b", cs.highlight.b),
                       Config::GetFloat(prefix + "highlight.a", cs.highlight.a)};

    ApplyCustom(cs);

    Config::Save();
    return true;
}

} // namespace XBase::Theme
