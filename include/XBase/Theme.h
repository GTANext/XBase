#pragma once

#include <string>
#include "imgui/imgui.h"

namespace XBase::Theme {

enum class Preset {
    Dark,
    Light,
    ClassicBlue,
    Custom
};

struct ColorSet {
    ImVec4 primary       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    ImVec4 accent        = ImVec4(0.95f, 0.37f, 0.22f, 1.00f);
    ImVec4 background    = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    ImVec4 surface       = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    ImVec4 text          = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    ImVec4 textDisabled  = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    ImVec4 border        = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
    ImVec4 highlight     = ImVec4(0.20f, 0.42f, 0.75f, 1.00f);
};

void Init();
void ApplyPreset(Preset preset);
void ApplyCustom(const ColorSet& colors);
ColorSet GetColors();

void PushStyle();
void PopStyle();

void LoadFont(const char* path, float size);
ImFont* GetDefaultFont();

} // namespace XBase::Theme
