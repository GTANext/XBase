#include <XBase/Theme.h>
#include <XBase/Log.h>

namespace XBase::Theme {

static ColorSet s_custom;

void ApplyPreset(Preset preset) {
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
    ApplyPreset(Preset::Dark);
}

void ApplyCustom(const ColorSet& colors) {
    s_custom = colors;
    PushStyle();
}

ColorSet GetColors() {
    ImGuiStyle& style = ImGui::GetStyle();
    ColorSet cs;
    cs.primary      = style.Colors[ImGuiCol_CheckMark];
    cs.accent       = style.Colors[ImGuiCol_ButtonActive];
    cs.background   = style.Colors[ImGuiCol_WindowBg];
    cs.surface      = style.Colors[ImGuiCol_FrameBg];
    cs.text         = style.Colors[ImGuiCol_Text];
    cs.textDisabled = style.Colors[ImGuiCol_TextDisabled];
    cs.border       = style.Colors[ImGuiCol_Border];
    cs.highlight    = style.Colors[ImGuiCol_HeaderActive];
    return cs;
}

void PushStyle() {
    ImGui::GetStyle() = ImGuiStyle();
    ApplyPreset(Preset::Dark);
}

void PopStyle() {
}

void LoadFont(const char* path, float size) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(path, size);
    if (font) {
        Log::Info(("Theme: loaded font " + std::string(path)).c_str());
    } else {
        Log::Warn(("Theme: failed to load font " + std::string(path)).c_str());
    }
}

ImFont* GetDefaultFont() {
    return ImGui::GetIO().Fonts->Fonts.empty() ? nullptr : ImGui::GetIO().Fonts->Fonts[0];
}

} // namespace XBase::Theme
