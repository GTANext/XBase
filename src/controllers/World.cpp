#include <XBase/World.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CWeather.h"
#include "CClock.h"
#include "CTimer.h"
#include "CPlayerPed.h"
#include "CPickups.h"
#include "CStreaming.h"
#include "CCarCtrl.h"
#include "extensions/ScriptCommands.h"
#include <ctime>

namespace {

bool s_weatherLocked = false;
int s_lockedWeather = 0;

bool s_freezeTime = false;
bool s_fasterClock = false;
bool s_noWaterPhysics = false;

} // namespace

namespace XBase::World {

void Process() {
    if (s_weatherLocked) {
        CWeather::ForceWeatherNow(static_cast<short>(s_lockedWeather));
    }

    if (s_freezeTime) {
        *reinterpret_cast<unsigned int*>(0xB7CB64) = 0;
    } else if (s_fasterClock) {
        *reinterpret_cast<unsigned int*>(0xB7CB64) = 10;
    } else {
        *reinterpret_cast<unsigned int*>(0xB7CB64) = 1000;
    }

    if (s_noWaterPhysics) {
        CWeather::UnderWaterness = 0.0f;
    }
}

void SetWeather(int id, bool lock) {
    CWeather::ForceWeatherNow(static_cast<short>(id));
    s_weatherLocked = lock;
    if (lock) s_lockedWeather = id;
}

void ReleaseWeather() {
    if (s_weatherLocked) {
        CWeather::ReleaseWeather();
        s_weatherLocked = false;
    }
}

bool IsWeatherLocked() {
    return s_weatherLocked;
}

void SetTime(int hour, int minute) {
    CClock::ms_nGameClockHours = hour;
    CClock::ms_nGameClockMinutes = minute;
}

void GetTime(int& hour, int& minute) {
    hour = CClock::ms_nGameClockHours;
    minute = CClock::ms_nGameClockMinutes;
}

void SyncTimeWithSystemClock() {
    const std::time_t nowTime = std::time(nullptr);
    const std::tm* now = std::localtime(&nowTime);
    if (!now) return;
    CClock::ms_nGameClockHours = now->tm_hour;
    CClock::ms_nGameClockMinutes = now->tm_min;
    CClock::ms_nGameClockSeconds = now->tm_sec;
    CClock::ms_nGameClockMonth = now->tm_mon + 1;
    CClock::ms_nGameClockDays = now->tm_mday;
    CClock::CurrentDay = now->tm_wday + 1;
}

void SetGameSpeed(float speed) {
    CTimer::ms_fTimeScale = speed;
}

float GetGameSpeed() {
    return CTimer::ms_fTimeScale;
}

int GetFpsLimit() {
    return RsGlobal.maxFPS;
}

void SetFpsLimit(int limit) {
    RsGlobal.maxFPS = limit;
}

float GetGravity() {
    return *reinterpret_cast<float*>(0x863984);
}

void SetGravity(float gravity) {
    *reinterpret_cast<float*>(0x863984) = gravity;
}

float GetRain() { return CWeather::Rain; }
void SetRain(float rain) { CWeather::Rain = rain; CWeather::bScriptsForceRain = true; }
float GetFoggyness() { return CWeather::Foggyness; }
void SetFoggyness(float fog) { CWeather::Foggyness = fog; }
float GetCloudCoverage() { return CWeather::CloudCoverage; }
void SetCloudCoverage(float clouds) { CWeather::CloudCoverage = clouds; }
float GetWind() { return CWeather::Wind; }
void SetWind(float wind) { CWeather::Wind = wind; }
float GetSandstorm() { return CWeather::Sandstorm; }
void SetSandstorm(float sandstorm) { CWeather::Sandstorm = sandstorm; }
float GetExtraSunnyness() { return CWeather::ExtraSunnyness; }
void SetExtraSunnyness(float sun) { CWeather::ExtraSunnyness = sun; }
float GetWetRoads() { return CWeather::WetRoads; }
void SetWetRoads(float wet) { CWeather::WetRoads = wet; }

void SetFreezeTime(bool enable) {
    s_freezeTime = enable;
    *reinterpret_cast<unsigned char*>(0xB70152) = enable ? 1 : 0;
}

bool IsTimeFrozen() {
    return *reinterpret_cast<unsigned char*>(0xB70152) != 0;
}

void SetFasterClock(bool enable) {
    s_fasterClock = enable;
}

bool IsFasterClock() {
    return s_fasterClock;
}

void SetDisableReplay(bool enable) {
    *reinterpret_cast<bool*>(0x969170) = enable;
}

bool IsReplayDisabled() {
    return *reinterpret_cast<bool*>(0x969170);
}

void SetDisableCheats(bool enable) {
    *reinterpret_cast<bool*>(0x96918C) = enable;
}

bool AreCheatsDisabled() {
    return *reinterpret_cast<bool*>(0x96918C);
}

void SetForbiddenAreaWanted(bool enable) {
    *reinterpret_cast<bool*>(0x96ABCC) = enable;
}

bool IsForbiddenAreaWanted() {
    return *reinterpret_cast<bool*>(0x96ABCC);
}

void SetFreePayNSpray(bool enable) {
    *reinterpret_cast<bool*>(0x96AC04) = enable;
}

bool IsFreePayNSpray() {
    return *reinterpret_cast<bool*>(0x96AC04);
}

void SetNoWaterPhysics(bool enable) {
    s_noWaterPhysics = enable;
}

bool IsNoWaterPhysics() {
    return s_noWaterPhysics;
}

void DestroyAllVehicles() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    for (CVehicle* v : CPools::ms_pVehiclePool) {
        if (v) v->BlowUpCar(player, false);
    }
}

void DestroyAllPeds() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    for (CPed* p : CPools::ms_pPedPool) {
        if (!p || p == player || p->m_fHealth <= 0.0f) continue;
        p->m_fHealth = 0.0f;
    }
}

int SpawnPickup(const Types::PickupOptions& options) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return -1;

    CVector pos = player->GetPosition();
    pos.x += 3.0f;

    CStreaming::RequestModel(static_cast<int>(options.modelId), PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);

    int handle = -1;
    plugin::Command<plugin::Commands::CREATE_PICKUP>(
        static_cast<int>(options.modelId),
        options.type,
        options.quantity,
        pos.x, pos.y, pos.z,
        &handle
    );

    if (handle >= 0) {
        plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(static_cast<int>(options.modelId));
    }
    return handle;
}

bool RemoveTrackedPickups() {
    CPickups::RemoveMissionPickUps();
    return true;
}

} // namespace XBase::World
