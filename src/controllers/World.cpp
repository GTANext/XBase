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
bool s_lockedTime = false;
int s_lockedHour = 0;
int s_lockedMinute = 0;
bool s_fasterClock = false;
bool s_noWaterPhysics = false;
bool s_hasClockIntervalSnapshot = false;
unsigned int s_savedClockInterval = 1000;
bool s_hasFreezeFlagSnapshot = false;
unsigned char s_savedFreezeFlag = 0;
bool s_hasUnderWaternessSnapshot = false;
float s_savedUnderWaterness = 0.0f;

void CaptureClockInterval() {
    if (s_hasClockIntervalSnapshot) return;
    s_savedClockInterval = *reinterpret_cast<unsigned int*>(0xB7CB64);
    s_hasClockIntervalSnapshot = true;
}

void CaptureFreezeFlag() {
    if (s_hasFreezeFlagSnapshot) return;
    s_savedFreezeFlag = *reinterpret_cast<unsigned char*>(0xB70152);
    s_hasFreezeFlagSnapshot = true;
}

bool HasClockOverride() {
    return s_freezeTime || s_lockedTime || s_fasterClock;
}

void RestoreClockInterval() {
    if (!s_hasClockIntervalSnapshot) return;
    *reinterpret_cast<unsigned int*>(0xB7CB64) = s_savedClockInterval;
    s_hasClockIntervalSnapshot = false;
}

void RestoreFreezeFlag() {
    if (!s_hasFreezeFlagSnapshot) return;
    *reinterpret_cast<unsigned char*>(0xB70152) = s_savedFreezeFlag;
    s_hasFreezeFlagSnapshot = false;
}

void RestoreClockState() {
    RestoreClockInterval();
    RestoreFreezeFlag();
}

} // namespace

namespace XBase::World {

void NotifyGameInit() {
    s_weatherLocked = false;
    s_freezeTime = false;
    s_lockedTime = false;
    s_fasterClock = false;
    s_noWaterPhysics = false;
    s_hasClockIntervalSnapshot = false;
    s_hasFreezeFlagSnapshot = false;
    s_hasUnderWaternessSnapshot = false;
}

void Shutdown() {
    ReleaseWeather();
    s_freezeTime = false;
    s_lockedTime = false;
    s_fasterClock = false;
    s_noWaterPhysics = false;
    RestoreClockState();
    if (s_hasUnderWaternessSnapshot) {
        CWeather::UnderWaterness = s_savedUnderWaterness;
        s_hasUnderWaternessSnapshot = false;
    }
}

void Process() {
    if (s_weatherLocked) {
        CWeather::ForceWeatherNow(static_cast<short>(s_lockedWeather));
    }

    if (s_freezeTime || s_lockedTime) {
        CClock::ms_nGameClockHours = s_lockedHour;
        CClock::ms_nGameClockMinutes = s_lockedMinute;
        *reinterpret_cast<unsigned int*>(0xB7CB64) = 0;
    } else if (s_fasterClock) {
        *reinterpret_cast<unsigned int*>(0xB7CB64) = 10;
    }

    if (s_noWaterPhysics) {
        CWeather::UnderWaterness = 0.0f;
    }
}

int GetWeather() { return static_cast<int>(CWeather::OldWeatherType); }

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
    if (s_freezeTime == enable) return;
    if (enable) {
        CaptureClockInterval();
        CaptureFreezeFlag();
        s_lockedHour = CClock::ms_nGameClockHours;
        s_lockedMinute = CClock::ms_nGameClockMinutes;
        *reinterpret_cast<unsigned char*>(0xB70152) = 1;
        s_freezeTime = true;
        return;
    }

    s_freezeTime = false;
    RestoreFreezeFlag();
    if (!HasClockOverride()) RestoreClockInterval();
}

bool IsTimeFrozen() {
    return s_freezeTime;
}

void SetLockedTime(bool enable, int hour, int minute) {
    if (enable) {
        CaptureClockInterval();
        s_lockedTime = true;
        s_lockedHour = hour;
        s_lockedMinute = minute;
        return;
    }

    s_lockedTime = false;
    if (!HasClockOverride()) RestoreClockInterval();
}

void SetFasterClock(bool enable) {
    if (s_fasterClock == enable) return;
    if (enable) {
        CaptureClockInterval();
        s_fasterClock = true;
        return;
    }

    s_fasterClock = false;
    if (!HasClockOverride()) RestoreClockInterval();
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
    if (s_noWaterPhysics == enable) return;
    if (enable) {
        s_savedUnderWaterness = CWeather::UnderWaterness;
        s_hasUnderWaternessSnapshot = true;
        s_noWaterPhysics = true;
        return;
    }

    s_noWaterPhysics = false;
    if (s_hasUnderWaternessSnapshot) {
        CWeather::UnderWaterness = s_savedUnderWaterness;
        s_hasUnderWaternessSnapshot = false;
    }
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

int GetDaysPassed() { return CClock::ms_nGameClockDays; }
void SetDaysPassed(int days) { CClock::ms_nGameClockDays = days; }

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

bool UpdateLastPickup(const Types::PickupOptions&) { return false; }
bool RemoveLastPickup() { return false; }

bool RemoveTrackedPickups() {
    CPickups::RemoveMissionPickUps();
    return true;
}

} // namespace XBase::World
