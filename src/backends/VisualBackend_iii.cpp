#include "VisualBackend.h"

#include "CWeather.h"
#include "CHud.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::VisualBackend {

namespace {
bool s_hasHudSnapshot = false;
bool s_savedHudVisible = true;
bool s_hasFilterSnapshot = false;
short s_savedOldWeather = 0;
short s_savedNewWeather = 0;
int s_currentFilter = 0;
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
    plugin::Command<plugin::Commands::DISPLAY_RADAR>(enable);
    return true;
}

bool SetFilter(int id, float) {
    if (id == 0) {
        if (s_currentFilter != 0) {
            if (s_hasFilterSnapshot) {
                CWeather::OldWeatherType = s_savedOldWeather;
                CWeather::NewWeatherType = s_savedNewWeather;
                s_hasFilterSnapshot = false;
            }
            s_currentFilter = 0;
        }
        return true;
    }
    if (id < 0) id = 0;
    if (id > 3) id = 3;
    if (!s_hasFilterSnapshot) {
        s_savedOldWeather = CWeather::OldWeatherType;
        s_savedNewWeather = CWeather::NewWeatherType;
        s_hasFilterSnapshot = true;
    }
    s_currentFilter = id;
    CWeather::OldWeatherType = static_cast<short>(id);
    CWeather::NewWeatherType = static_cast<short>(id);
    return true;
}

void SetRadarOptions(const Visual::RadarOptions&) {}

void Init() {
    s_hasHudSnapshot = false;
    s_hasFilterSnapshot = false;
    s_currentFilter = 0;
}

void Shutdown() {
    if (s_hasFilterSnapshot) {
        CWeather::OldWeatherType = s_savedOldWeather;
        CWeather::NewWeatherType = s_savedNewWeather;
        s_hasFilterSnapshot = false;
    }
    if (s_hasHudSnapshot) {
        CHud::m_Wants_To_Draw_Hud = s_savedHudVisible;
        s_hasHudSnapshot = false;
    }
    s_currentFilter = 0;
}

void Process() {
    if (s_currentFilter != 0) {
        CWeather::OldWeatherType = static_cast<short>(s_currentFilter);
        CWeather::NewWeatherType = static_cast<short>(s_currentFilter);
    }
}

} // namespace XBase::Detail::VisualBackend