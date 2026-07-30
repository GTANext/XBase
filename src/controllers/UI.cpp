#include <XBase/UI.h>
#include <XBase/Theme.h>
#include <XBase/Hooks.h>
#include <XBase/Log.h>
#include <imgui.h>
#include <cstdio>

namespace XBase::UI {

namespace {

std::string g_title = "XBase";
bool g_initialized = false;
bool g_windowOpen = true;

std::vector<WindowEntry> g_windows;

bool g_inTabBar = false;
std::string g_tabBarName;
std::vector<TabItem> g_tabItems;
int g_activeTab = 0;

WindowEntry* FindWindow(const std::string& name) {
    for (auto& w : g_windows) {
        if (w.name == name) return &w;
    }
    return nullptr;
}

void DrawWindows() {
    for (auto& w : g_windows) {
        if (!w.open) continue;
        bool isOpen = true;
        if (ImGui::Begin(w.name.c_str(), &isOpen, w.flags)) {
            if (w.drawFn) w.drawFn();
        }
        ImGui::End();
        if (!isOpen) w.open = false;
    }
}

void DefaultMenu() {
    if (!g_windowOpen) return;

    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(g_title.c_str(), &g_windowOpen, ImGuiWindowFlags_NoCollapse);

    if (!g_tabItems.empty() && g_inTabBar) {
        if (ImGui::BeginTabBar(g_tabBarName.c_str())) {
            for (int i = 0; i < static_cast<int>(g_tabItems.size()); ++i) {
                bool selected = (g_activeTab == i);
                if (ImGui::BeginTabItem(g_tabItems[i].label.c_str(), &selected)) {
                    g_activeTab = i;
                    if (g_tabItems[i].drawFn) g_tabItems[i].drawFn();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    } else {
        for (auto& w : g_windows) {
            if (w.drawFn) w.drawFn();
        }
    }

    ImGui::End();
}

} // namespace

void Init(const std::string& title) {
    if (g_initialized) return;

    g_title = title;
    g_initialized = true;

    Theme::Init();
    Hooks::SetDrawCallback(DrawWindows);
    Hooks::SetMenuVisible(g_windowOpen);

    Log::Info("UI: initialized");
}

void Process() {
    for (auto& w : g_windows) {
        if (w.open && w.drawFn) {
            w.drawFn();
        }
    }
}

void Shutdown() {
    g_windows.clear();
    g_tabItems.clear();
    g_initialized = false;
}

void AddWindow(const std::string& name, std::function<void()> drawFn, bool defaultOpen, ImGuiWindowFlags flags) {
    if (FindWindow(name)) return;
    g_windows.push_back({ name, std::move(drawFn), defaultOpen, flags });
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
    WindowEntry* w = FindWindow(name);
    if (w) w->open = visible;
}

bool IsWindowVisible(const std::string& name) {
    WindowEntry* w = FindWindow(name);
    return w ? w->open : false;
}

void ToggleWindow(const std::string& name) {
    WindowEntry* w = FindWindow(name);
    if (w) w->open = !w->open;
}

void BeginTabBar(const std::string& name) {
    g_inTabBar = true;
    g_tabBarName = name;
    g_tabItems.clear();
    g_activeTab = 0;
}

void AddTab(const std::string& label, std::function<void()> drawFn) {
    g_tabItems.push_back({ label, std::move(drawFn) });
}

bool RenderTabBar(float height) {
    if (g_tabItems.empty()) return false;

    if (height > 0.0f) {
        ImGui::BeginChild("TabBody", ImVec2(0, height), true);
    }

    bool changed = false;
    if (ImGui::BeginTabBar(g_tabBarName.c_str())) {
        for (int i = 0; i < static_cast<int>(g_tabItems.size()); ++i) {
            bool selected = (g_activeTab == i);
            if (ImGui::BeginTabItem(g_tabItems[i].label.c_str())) {
                if (g_activeTab != i) {
                    g_activeTab = i;
                    changed = true;
                }
                if (g_tabItems[i].drawFn) g_tabItems[i].drawFn();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    if (height > 0.0f) {
        ImGui::EndChild();
    }

    return changed;
}

void EndTabBar() {
    g_inTabBar = false;
    g_tabItems.clear();
}

void CenterText(const char* text) {
    if (!text) return;
    float w = ImGui::CalcTextSize(text).x;
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((avail - w) * 0.5f);
    ImGui::Text("%s", text);
}

void SeparatorText(const char* label) {
    ImGui::Separator();
    if (label && label[0]) {
        ImGui::Text("  %s", label);
        ImGui::Separator();
    }
}

bool StyledButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::GetColors().primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::GetColors().highlight);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::GetColors().accent);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void BeginGroupBox(const char* label, ImVec2 size) {
    ImGui::BeginChild(label, size, true, ImGuiWindowFlags_NoScrollbar);
    if (label && label[0]) {
        ImGui::Text("%s", label);
        ImGui::Separator();
    }
}

void EndGroupBox() {
    ImGui::EndChild();
}

void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace XBase::UI
