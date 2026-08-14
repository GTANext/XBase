#include <XBase/World.h>
#include <XBase/Core.h>

#include "../backends/WorldBackend.h"

#include <ctime>
#include <vector>

namespace XBase::World {

namespace {
bool s_weatherLocked = false;
int s_lockedWeather = 0;
bool s_freezeTime = false;
bool s_lockedTime = false;
int s_lockedHour = 0;
int s_lockedMinute = 0;
bool s_fasterClock = false;
bool s_disableReplay = false;
bool s_disableCheats = false;
std::vector<int> s_trackedPickups;
int s_lastPickupHandle = -1;
float s_lastPickupX = 0.0f;
float s_lastPickupY = 0.0f;
float s_lastPickupZ = 0.0f;

void RestoreTimeOverrides() {
    if (!s_fasterClock) return;
    Detail::WorldBackend::SetFastClock(false);
    s_fasterClock = false;
}
} // namespace

void NotifyGameInit() {
    s_weatherLocked = false;
    s_freezeTime = false;
    s_lockedTime = false;
    s_fasterClock = false;
    s_disableReplay = false;
    s_disableCheats = false;
    s_lastPickupHandle = -1;
    s_trackedPickups.clear();
}

void Shutdown() {
    if (s_weatherLocked) {
        Detail::WorldBackend::ReleaseWeather();
        s_weatherLocked = false;
    }
    if (s_fasterClock) RestoreTimeOverrides();
    if (s_disableReplay) Detail::WorldBackend::SetDisableReplay(false);
    if (s_disableCheats) Detail::WorldBackend::SetDisableCheats(false);
    if (s_lastPickupHandle >= 0) {
        Detail::WorldBackend::RemovePickup(s_lastPickupHandle);
        s_lastPickupHandle = -1;
    }
}

void Process() {
    if (!Core::IsWorldReady()) return;

    if (s_weatherLocked) {
        Detail::WorldBackend::ForceWeather(static_cast<short>(s_lockedWeather));
    }

    if (s_freezeTime || s_lockedTime) {
        Detail::WorldBackend::SetGameClock(s_lockedHour, s_lockedMinute);
    }
}

int GetWeather() { return static_cast<int>(Detail::WorldBackend::GetOldWeather()); }

void SetWeather(int id, bool lock) {
    Detail::WorldBackend::ForceWeather(static_cast<short>(id));
    s_weatherLocked = lock;
    if (lock) s_lockedWeather = id;
}

void ReleaseWeather() {
    if (s_weatherLocked) {
        Detail::WorldBackend::ReleaseWeather();
        s_weatherLocked = false;
    }
}

bool IsWeatherLocked() { return s_weatherLocked; }

void SetTime(int hour, int minute) {
    Detail::WorldBackend::SetGameClock(hour, minute);
}

void GetTime(int& hour, int& minute) {
    Detail::WorldBackend::GetTime(hour, minute);
}

void SyncTimeWithSystemClock() {
    const std::time_t nowTime = std::time(nullptr);
    const std::tm* now = std::localtime(&nowTime);
    if (!now) return;
    Detail::WorldBackend::SetGameClock(now->tm_hour, now->tm_min);
}

void SetGameSpeed(float speed) { Detail::WorldBackend::SetTimeScale(speed); }
float GetGameSpeed() { return Detail::WorldBackend::GetTimeScale(); }

int GetFpsLimit() { return Detail::WorldBackend::GetMaxFps(); }
void SetFpsLimit(int limit) { Detail::WorldBackend::SetMaxFps(limit); }

float GetGravity() { return Detail::WorldBackend::GetGravity(); }
void SetGravity(float gravity) { Detail::WorldBackend::SetGravity(gravity); }

float GetRain() { return 0.0f; }
void SetRain(float) {}
float GetFoggyness() { return 0.0f; }
void SetFoggyness(float) {}
float GetCloudCoverage() { return 0.0f; }
void SetCloudCoverage(float) {}
float GetWind() { return 0.0f; }
void SetWind(float) {}
float GetSandstorm() { return 0.0f; }
void SetSandstorm(float) {}
float GetExtraSunnyness() { return 0.0f; }
void SetExtraSunnyness(float) {}
float GetWetRoads() { return 0.0f; }
void SetWetRoads(float) {}

void DestroyAllVehicles() { Detail::WorldBackend::DestroyAllVehicles(); }
void DestroyAllPeds() { Detail::WorldBackend::DestroyAllPeds(); }

void SetFreezeTime(bool enable) {
    if (s_freezeTime == enable) return;
    if (enable) {
        int hour = 0;
        int minute = 0;
        Detail::WorldBackend::GetTime(hour, minute);
        s_lockedHour = hour;
        s_lockedMinute = minute;
        s_freezeTime = true;
        return;
    }
    s_freezeTime = false;
    if (!s_lockedTime && !s_fasterClock) {
        Detail::WorldBackend::SetGameClock(s_lockedHour, s_lockedMinute);
    }
}

bool IsTimeFrozen() { return s_freezeTime; }

void SetLockedTime(bool enable, int hour, int minute) {
    s_lockedTime = enable;
    if (enable) {
        s_lockedHour = hour;
        s_lockedMinute = minute;
    }
}

void SetFasterClock(bool enable) {
    if (s_fasterClock == enable) return;
    s_fasterClock = enable;
    Detail::WorldBackend::SetFastClock(enable);
}

bool IsFasterClock() { return s_fasterClock; }

void SetDisableReplay(bool enable) {
    if (s_disableReplay == enable) return;
    s_disableReplay = enable;
    Detail::WorldBackend::SetDisableReplay(enable);
}

bool IsReplayDisabled() { return Detail::WorldBackend::IsReplayDisabled(); }

void SetDisableCheats(bool enable) {
    if (s_disableCheats == enable) return;
    s_disableCheats = enable;
    Detail::WorldBackend::SetDisableCheats(enable);
}

bool AreCheatsDisabled() { return Detail::WorldBackend::AreCheatsDisabled(); }

void SetForbiddenAreaWanted(bool) {}
bool IsForbiddenAreaWanted() { return false; }
void SetFreePayNSpray(bool) {}
bool IsFreePayNSpray() { return false; }
void SetNoWaterPhysics(bool) {}
bool IsNoWaterPhysics() { return false; }
void SetSolidWater(bool) {}

int GetDaysPassed() { return Detail::WorldBackend::GetDaysPassed(); }
void SetDaysPassed(int days) { Detail::WorldBackend::SetDaysPassed(days); }

int SpawnPickup(const Types::PickupOptions& options) {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    const int handle = Detail::WorldBackend::SpawnPickupNearPlayer(options, x, y, z);
    if (handle >= 0) {
        s_lastPickupHandle = handle;
        s_lastPickupX = x;
        s_lastPickupY = y;
        s_lastPickupZ = z;
        s_trackedPickups.push_back(handle);
    }
    return handle;
}

bool UpdateLastPickup(const Types::PickupOptions& options) {
    if (s_lastPickupHandle < 0) return false;
    Detail::WorldBackend::RemovePickup(s_lastPickupHandle);
    for (auto it = s_trackedPickups.begin(); it != s_trackedPickups.end(); ++it) {
        if (*it == s_lastPickupHandle) {
            s_trackedPickups.erase(it);
            break;
        }
    }
    const int handle = Detail::WorldBackend::SpawnPickupAt(options, s_lastPickupX, s_lastPickupY, s_lastPickupZ);
    if (handle < 0) {
        s_lastPickupHandle = -1;
        return false;
    }
    s_lastPickupHandle = handle;
    s_trackedPickups.push_back(handle);
    return true;
}

bool RemoveLastPickup() {
    if (s_lastPickupHandle < 0) return false;
    Detail::WorldBackend::RemovePickup(s_lastPickupHandle);
    for (auto it = s_trackedPickups.begin(); it != s_trackedPickups.end(); ++it) {
        if (*it == s_lastPickupHandle) {
            s_trackedPickups.erase(it);
            break;
        }
    }
    s_lastPickupHandle = -1;
    return true;
}

bool RemoveTrackedPickups() {
    int removed = 0;
    for (const int handle : s_trackedPickups) {
        if (handle < 0) continue;
        Detail::WorldBackend::RemovePickup(handle);
        ++removed;
    }
    s_trackedPickups.clear();
    return removed > 0;
}

} // namespace XBase::World