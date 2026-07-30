#include <XBase/Visual.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CHud.h"
#include "CPostEffects.h"

namespace XBase::Visual {

static int s_currentFilter = 0;
static float s_filterStrength = 0.0f;

void DisplayHud(bool enable) {
    CHud::m_Wants_To_Draw_Hud = enable;
}

void DisplayRadar(bool enable) {
    CHud::bScriptDontDisplayRadar = !enable;
}

void SetFilter(int id, float strength) {
    s_currentFilter = id;
    s_filterStrength = strength;
}

void Process() {
    CPostEffects::m_bNightVision = false;
    CPostEffects::m_bInfraredVision = false;
    CPostEffects::m_bDarknessFilter = false;
    CPostEffects::m_bHeatHazeFX = false;

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
