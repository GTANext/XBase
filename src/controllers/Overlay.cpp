#include <XBase/Overlay.h>

#include <XBase/Core.h>
#include <XBase/Hooks.h>
#include <XBase/Player.h>
#include <XBase/Teleport.h>
#include <XBase/UI.h>
#include <XBase/ValueTypes.h>
#include <XBase/World.h>

#include <cstdio>

namespace {

bool s_visible = false;
bool s_topLeft = true;
bool s_topRight = false;
bool s_bottomLeft = false;
bool s_bottomRight = false;

XBase::Hooks::DrawCallbackId s_drawCallback;

constexpr float kPadding = 12.0f;
constexpr float kLineHeight = 16.0f;
constexpr int kMaxLines = 8;

void DrawOverlay() {
    if (!s_visible || !XBase::Core::IsWorldReady()) {
        return;
    }

    char lines[kMaxLines][64] = {};
    int count = 0;

    std::snprintf(lines[count++], sizeof(lines[0]), "FPS: %.0f", XBase::UI::GetFrameRate());

    const XBase::Vec3 position = XBase::Teleport::GetCurrentPosition();
    std::snprintf(
        lines[count++],
        sizeof(lines[0]),
        "Pos: %.1f %.1f %.1f",
        position.x,
        position.y,
        position.z);

    std::snprintf(
        lines[count++],
        sizeof(lines[0]),
        "H: %.0f A: %.0f",
        XBase::Player::GetHealth(),
        XBase::Player::GetArmour());

    std::snprintf(
        lines[count++],
        sizeof(lines[0]),
        "$%d W: %d",
        XBase::Player::GetMoney(),
        XBase::Player::GetWantedLevel());

    int hour = 0;
    int minute = 0;
    XBase::World::GetTime(hour, minute);
    std::snprintf(lines[count++], sizeof(lines[0]), "Time: %02d:%02d", hour, minute);

    const XBase::Vec2 display = XBase::UI::GetDisplaySize();
    const bool rightAligned = s_topRight || s_bottomRight;
    const bool bottomAligned = s_bottomLeft || s_bottomRight;

    const float x = rightAligned ? display.x - kPadding : kPadding;
    const float y = bottomAligned
        ? display.y - kPadding - static_cast<float>(count) * kLineHeight
        : kPadding;

    const XBase::Color white{};
    for (int index = 0; index < count; ++index) {
        const float lineY = y + static_cast<float>(index) * kLineHeight;
        XBase::UI::Canvas::Text({x, lineY}, white, lines[index]);
    }
}

} // namespace

namespace XBase::Overlay {

void Init() {
    s_visible = false;
    // 三个版本统一走 ImGui 画布，不碰游戏自带的字体系统，
    // 各版本的字体接口与字符编码差异很大，复用画布可以一份代码覆盖全部版本
    if (!s_drawCallback) {
        s_drawCallback = Hooks::RegisterDrawCallback(Draw);
    }
}

void Process() {
}

void Shutdown() {
    if (s_drawCallback) {
        Hooks::UnregisterDrawCallback(s_drawCallback);
        s_drawCallback = {};
    }
    s_visible = false;
    s_topLeft = true;
    s_topRight = false;
    s_bottomLeft = false;
    s_bottomRight = false;
}

void Draw() {
    DrawOverlay();
}

void SetVisible(bool enable) {
    s_visible = enable;
}

bool IsVisible() {
    return s_visible;
}

void Toggle() {
    s_visible = !s_visible;
}

void SetPosition(bool topLeft, bool topRight, bool bottomLeft, bool bottomRight) {
    s_topLeft = topLeft;
    s_topRight = topRight;
    s_bottomLeft = bottomLeft;
    s_bottomRight = bottomRight;
}

} // namespace XBase::Overlay
