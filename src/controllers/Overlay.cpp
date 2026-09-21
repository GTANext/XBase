#include <XBase/Overlay.h>
#include <XBase/Core.h>
#include <XBase/Player.h>
#include <XBase/Vehicle.h>
#include <XBase/World.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CTimer.h"
#include "CFont.h"
#include "CRGBA.h"
#include "RenderWare.h"
#include <cstdio>
#include <cstring>

namespace {

bool s_visible = false;
bool s_topLeft = true, s_topRight = false, s_bottomLeft = false, s_bottomRight = false;

int CountLines(const char* text) {
    if (!text || !text[0]) return 0;
    int lines = 1;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') ++lines;
    }
    return lines;
}

void DrawBlock(const char* text, float x, float y, CRGBA color, bool rightAligned) {
    if (!text || !text[0]) return;
    CFont::SetScale(0.35f, 0.35f);
    CFont::SetColor(color);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetBackground(false, false);
    CFont::SetWrapx(0.0f);
    CFont::SetCentreSize(0.0f);

    float lineY = y;
    const char* p = text;
    while (*p) {
        const char* nl = std::strchr(p, '\n');
        int len = nl ? static_cast<int>(nl - p) : static_cast<int>(std::strlen(p));
        if (len > 0) {
            char line[256];
            std::strncpy(line, p, len);
            line[len] = '\0';
            const float lineX = rightAligned ? x - CFont::GetStringWidth(line, true) : x;
            CFont::PrintString(lineX, lineY, line);
        }
        if (!nl) break;
        p = nl + 1;
        lineY += 14.0f;
    }
}

} // namespace

namespace XBase::Overlay {

void Init() {
    s_visible = false;
}

void Process() {
}

void Shutdown() {
    s_visible = false;
    s_topLeft = true;
    s_topRight = false;
    s_bottomLeft = false;
    s_bottomRight = false;
}

void Draw() {
    if (!s_visible || !Core::IsWorldReady()) return;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    char buf[512];
    CVector pos = player->GetPosition();
    int hour, minute;
    World::GetTime(hour, minute);

    int fps = static_cast<int>(1.0f / CTimer::ms_fTimeStep * 60.0f);
    float health = Player::GetHealth();
    float armour = Player::GetArmour();
    int money = Player::GetMoney();
    int wanted = Player::GetWantedLevel();

    std::snprintf(buf, sizeof(buf),
        "FPS: %d\nPos: %.1f %.1f %.1f\nH: %.0f A: %.0f\n$%d W: %d\nTime: %02d:%02d\nInterior: %d",
        fps, pos.x, pos.y, pos.z, health, armour, money, wanted, hour, minute, player->m_nAreaCode);

    CRGBA white(255, 255, 255, 255);

    constexpr float padding = 10.0f;
    constexpr float lineHeight = 14.0f;
    const float screenWidth = static_cast<float>(RsGlobal.maximumWidth);
    const float screenHeight = static_cast<float>(RsGlobal.maximumHeight);

    const bool rightAligned = s_topRight || s_bottomRight;
    const bool bottomAligned = s_bottomLeft || s_bottomRight;
    const float x = rightAligned ? screenWidth - padding : padding;
    const float y = bottomAligned
        ? screenHeight - padding - static_cast<float>(CountLines(buf)) * lineHeight
        : 30.0f;

    DrawBlock(buf, x, y, white, rightAligned);
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
