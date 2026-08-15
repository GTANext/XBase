#include <XBase/Visual.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "extensions/ScriptCommands.h"
#include "CHud.h"
#include "CPostEffects.h"

#include <array>

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

RadarOptions s_radarOptions;
bool s_hasRadarSnapshotNv = false;
bool s_savedNightVisionRadar = false;
bool s_savedInfraredRadar = false;

struct PatchSnapshot {
    uintptr_t address = 0;
    std::array<unsigned char, 8> bytes{};
    std::size_t size = 0;
    bool captured = false;

    void Capture() {
        if (captured || address == 0 || size == 0) return;
        plugin::patch::GetRaw(address, bytes.data(), size);
        captured = true;
    }

    void Restore() {
        if (!captured) return;
        plugin::patch::SetRaw(address, bytes.data(), size);
        captured = false;
    }
};

void ApplyNops(std::array<PatchSnapshot, 12>& sites) {
    for (PatchSnapshot& site : sites) {
        site.Capture();
        plugin::patch::Nop(site.address, site.size);
    }
}

void RestoreSites(std::array<PatchSnapshot, 12>& sites) {
    for (PatchSnapshot& site : sites) {
        site.Restore();
    }
}

std::array<PatchSnapshot, 12> s_fullscreenSites = {{
    {0x575BF6, {}, 5, false}, {0x575C40, {}, 5, false}, {0x575C84, {}, 5, false}, {0x575CCE, {}, 5, false},
    {0x575D1F, {}, 5, false}, {0x575D6F, {}, 5, false}, {0x575DC2, {}, 5, false}, {0x575E12, {}, 5, false},
    {0x5754EC, {}, 6, false}, {0x575537, {}, 6, false}, {0x575311, {}, 6, false}, {0x575361, {}, 6, false},
}};
bool s_fullscreenApplied = false;

std::array<PatchSnapshot, 6> s_radarRotSites = {{
    {0x5837FB, {}, 6, false}, {0x583805, {}, 6, false}, {0x58380D, {}, 6, false},
    {0x5837D6, {}, 6, false}, {0x5837D0, {}, 6, false}, {0x5837C6, {}, 8, false},
}};
bool s_radarRotApplied = false;
bool s_unfogApplied = false;
bool s_unfogValue = false;

PatchSnapshot s_squareRadarSite{0x58585C, {}, 4, false};
bool s_squareApplied = false;
static float s_squareRadarScale = 0.000001f;
bool s_hideAreaNamesApplied = false;
bool s_hideVehicleNamesApplied = false;

void RestoreRadarPatches() {
    if (s_fullscreenApplied) {
        RestoreSites(s_fullscreenSites);
        s_fullscreenApplied = false;
    }
    if (s_radarRotApplied) {
        for (PatchSnapshot& site : s_radarRotSites) {
            site.Restore();
        }
        s_radarRotApplied = false;
    }
    if (s_unfogApplied) {
        plugin::patch::SetUChar(0xBA372C, 0x00);
        s_unfogApplied = false;
        s_unfogValue = false;
    }
    if (s_squareApplied) {
        s_squareRadarSite.Restore();
        s_squareApplied = false;
    }
}

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
    s_radarOptions = RadarOptions{};
    s_hideAreaNamesApplied = false;
    s_hideVehicleNamesApplied = false;
}

void Shutdown() {
    RestoreFilterState();
    RestoreHudRadarState();
    RestoreRadarPatches();
    s_currentFilter = 0;
    s_filterStrength = 0.0f;
    s_radarOptions = RadarOptions{};
}

void SetRadarOptions(const RadarOptions& options) {
    s_radarOptions = options;
}

void Process() {
    const RadarOptions& radar = s_radarOptions;

    if (radar.fullscreenMap != s_fullscreenApplied) {
        if (radar.fullscreenMap) {
            ApplyNops(s_fullscreenSites);
        } else {
            RestoreSites(s_fullscreenSites);
        }
        s_fullscreenApplied = radar.fullscreenMap;
    }

    if (radar.noRadarRot != s_radarRotApplied) {
        if (radar.noRadarRot) {
            plugin::patch::SetFloat(0xBA8310, 0.0f);
            plugin::patch::SetFloat(0xBA830C, 0.0f);
            plugin::patch::SetFloat(0xBA8308, 1.0f);
            for (PatchSnapshot& site : s_radarRotSites) {
                site.Capture();
                plugin::patch::Nop(site.address, site.size);
            }
        } else {
            for (PatchSnapshot& site : s_radarRotSites) {
                site.Restore();
            }
        }
        s_radarRotApplied = radar.noRadarRot;
    }

    if (radar.unfogMap != s_unfogValue) {
        s_unfogValue = radar.unfogMap;
        plugin::patch::SetUChar(0xBA372C, radar.unfogMap ? 0x50 : 0x00);
        s_unfogApplied = radar.unfogMap;
    }

    if (!s_hasRadarSnapshotNv) {
        s_savedNightVisionRadar = *reinterpret_cast<bool*>(0xC402B8);
        s_savedInfraredRadar = *reinterpret_cast<bool*>(0xC402B9);
        s_hasRadarSnapshotNv = true;
    }
    *reinterpret_cast<bool*>(0xC402B8) = radar.nightVision;
    *reinterpret_cast<bool*>(0xC402B9) = radar.infrared;

    CHud::bScriptDontDisplayAreaName = radar.hideAreaNames;
    CHud::bScriptDontDisplayVehicleName = radar.hideVehicleNames;
    if (radar.hideAreaNames != s_hideAreaNamesApplied) {
        plugin::Command<plugin::Commands::DISPLAY_ZONE_NAMES>(!radar.hideAreaNames);
        s_hideAreaNamesApplied = radar.hideAreaNames;
    }
    if (radar.hideVehicleNames != s_hideVehicleNamesApplied) {
        plugin::Command<plugin::Commands::DISPLAY_CAR_NAMES>(!radar.hideVehicleNames);
        s_hideVehicleNamesApplied = radar.hideVehicleNames;
    }

    if (radar.square != s_squareApplied) {
        if (radar.square) {
            s_squareRadarSite.Capture();
            plugin::patch::Set(s_squareRadarSite.address, &s_squareRadarScale);
        } else {
            s_squareRadarSite.Restore();
        }
        s_squareApplied = radar.square;
    }

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
