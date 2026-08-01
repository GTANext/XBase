#include <XBase/Visual.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CHud.h"
#include "CPostEffects.h"

namespace XBase::Visual {
namespace {

int s_currentFilter = 0;
float s_filterStrength = 0.0f;
bool s_savedNightVision = false;
bool s_savedInfraredVision = false;
bool s_savedDarknessFilter = false;
int s_savedDarknessAlpha = 0;
bool s_hasFilterSnapshot = false;
bool s_savedHudVisible = true;
bool s_hasHudSnapshot = false;
bool s_savedRadarHidden = false;
bool s_hasRadarSnapshot = false;

void CaptureFilterState() {
    if (s_hasFilterSnapshot) return;
    s_savedNightVision = CPostEffects::m_bNightVision;
    s_savedInfraredVision = CPostEffects::m_bInfraredVision;
    s_savedDarknessFilter = CPostEffects::m_bDarknessFilter;
    s_savedDarknessAlpha = CPostEffects::m_DarknessFilterAlpha;
    s_hasFilterSnapshot = true;
}

void RestoreFilterState() {
    if (!s_hasFilterSnapshot) return;
    CPostEffects::m_bNightVision = s_savedNightVision;
    CPostEffects::m_bInfraredVision = s_savedInfraredVision;
    CPostEffects::m_bDarknessFilter = s_savedDarknessFilter;
    CPostEffects::m_DarknessFilterAlpha = s_savedDarknessAlpha;
    s_hasFilterSnapshot = false;
}

void RestoreHudRadarState() {
    if (s_hasHudSnapshot) {
        CHud::m_Wants_To_Draw_Hud = s_savedHudVisible;
        s_hasHudSnapshot = false;
    }
    if (s_hasRadarSnapshot) {
        CHud::bScriptDontDisplayRadar = s_savedRadarHidden;
        s_hasRadarSnapshot = false;
    }
}

} // namespace

bool DisplayHud(bool enable) {
    if (!s_hasHudSnapshot) {
        s_savedHudVisible = CHud::m_Wants_To_Draw_Hud;
        s_hasHudSnapshot = true;
    }
    CHud::m_Wants_To_Draw_Hud = enable;
    return true;
}

bool DisplayRadar(bool enable) {
    if (!s_hasRadarSnapshot) {
        s_savedRadarHidden = CHud::bScriptDontDisplayRadar;
        s_hasRadarSnapshot = true;
    }
    CHud::bScriptDontDisplayRadar = !enable;
    return true;
}

bool SetFilter(int id, float strength) {
    if (id == 0) {
        RestoreFilterState();
        s_currentFilter = 0;
        s_filterStrength = 0.0f;
        return true;
    }
    if (id < 1 || id > 3) return false;
    CaptureFilterState();
    s_currentFilter = id;
    s_filterStrength = strength;
    return true;
}

void NotifyGameInit() {
    s_currentFilter = 0;
    s_filterStrength = 0.0f;
    s_hasFilterSnapshot = false;
    s_hasHudSnapshot = false;
    s_hasRadarSnapshot = false;
}

void Shutdown() {
    RestoreFilterState();
    RestoreHudRadarState();
    s_currentFilter = 0;
    s_filterStrength = 0.0f;
}

void Process() {
    if (s_currentFilter == 0) return;

    CPostEffects::m_bNightVision = false;
    CPostEffects::m_bInfraredVision = false;
    CPostEffects::m_bDarknessFilter = false;

    switch (s_currentFilter) {
    case 1:
        CPostEffects::ScriptNightVisionSwitch(true);
        break;
    case 2:
        CPostEffects::ScriptInfraredVisionSwitch(true);
        break;
    case 3: {
        int alpha = static_cast<int>(s_filterStrength * 255.0f);
        CPostEffects::ScriptDarknessFilterSwitch(true, alpha);
        break;
    }
    }
}

} // namespace XBase::Visual
