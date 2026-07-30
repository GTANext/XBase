#pragma once

#include <string>
#include <functional>
#include <vector>
#include "imgui/imgui.h"

namespace XBase::UI {

struct WindowEntry {
    std::string name;
    std::function<void()> drawFn;
    bool open;
    ImGuiWindowFlags flags;
};

void Init(const std::string& title = "XBase");
void Process();
void Shutdown();

void AddWindow(const std::string& name, std::function<void()> drawFn, bool defaultOpen = false, ImGuiWindowFlags flags = 0);
void RemoveWindow(const std::string& name);
void SetWindowVisible(const std::string& name, bool visible);
bool IsWindowVisible(const std::string& name);
void ToggleWindow(const std::string& name);

struct TabItem {
    std::string label;
    std::function<void()> drawFn;
};

void BeginTabBar(const std::string& name);
void AddTab(const std::string& label, std::function<void()> drawFn);
bool RenderTabBar(float height = 0.0f);
void EndTabBar();

void CenterText(const char* text);
void SeparatorText(const char* label);
bool StyledButton(const char* label, ImVec2 size = ImVec2(0, 0));
void BeginGroupBox(const char* label, ImVec2 size = ImVec2(0, 0));
void EndGroupBox();
void HelpMarker(const char* desc);

} // namespace XBase::UI
