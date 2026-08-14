#include "WorldBackend.h"

#include "CClock.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CStats.h"
#include "CStreaming.h"
#include "CWeather.h"
#include "CPickups.h"
#include "CTimer.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::WorldBackend {

void GetTime(int& hour, int& minute) {
    hour = CClock::ms_nGameClockHours;
    minute = CClock::ms_nGameClockMinutes;
}

void SetGameClock(int hour, int minute) {
    CClock::SetGameClock(static_cast<unsigned char>(hour), static_cast<unsigned char>(minute));
}

short GetOldWeather() {
    return CWeather::OldWeatherType;
}

void ForceWeather(short weather) {
    CWeather::ForceWeatherNow(weather);
}

void ReleaseWeather() {
    CWeather::ReleaseWeather();
}

float GetTimeScale() {
    return CTimer::ms_fTimeScale;
}

void SetTimeScale(float speed) {
    CTimer::ms_fTimeScale = speed;
}

int GetMaxFps() {
    return RsGlobal.maxFPS;
}

void SetMaxFps(int limit) {
    RsGlobal.maxFPS = limit;
}

float GetGravity() {
    return *reinterpret_cast<float*>(0x5F68D4);
}

void SetGravity(float gravity) {
    *reinterpret_cast<float*>(0x5F68D4) = gravity;
}

int GetDaysPassed() {
    return CStats::DaysPassed;
}

void SetDaysPassed(int days) {
    CStats::DaysPassed = days;
}

void DestroyAllVehicles() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    for (CVehicle* vehicle : CPools::ms_pVehiclePool) {
        if (vehicle) vehicle->BlowUpCar(player);
    }
}

void DestroyAllPeds() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    for (CPed* ped : CPools::ms_pPedPool) {
        if (!ped || ped == player || ped->m_fHealth <= 0.0f) continue;
        ped->m_fHealth = 0.0f;
    }
}

namespace {
bool s_replayByteCaptured = false;
unsigned char s_replayByte = 0;
bool s_cheatBytesCaptured = false;
unsigned char s_cheatFirst[5]{};
unsigned char s_cheatSecond[5]{};
bool s_fastClockCaptured = false;
bool s_fastClockValue = false;
} // namespace

bool IsReplayDisabled() {
    return plugin::patch::GetUChar(0x593170) == 0xC3;
}

void SetDisableReplay(bool enable) {
    if (!s_replayByteCaptured) {
        s_replayByte = plugin::patch::GetUChar(0x593170);
        s_replayByteCaptured = true;
    }
    plugin::patch::SetUChar(0x593170, enable ? 0xC3 : s_replayByte);
}

bool AreCheatsDisabled() {
    return plugin::patch::GetUChar(0x5841B8) == 0x90;
}

void SetDisableCheats(bool enable) {
    if (!s_cheatBytesCaptured) {
        plugin::patch::GetRaw(0x5841B8, s_cheatFirst, sizeof(s_cheatFirst));
        plugin::patch::GetRaw(0x5841C7, s_cheatSecond, sizeof(s_cheatSecond));
        s_cheatBytesCaptured = true;
    }
    if (enable) {
        plugin::patch::Nop(0x5841B8, 5);
        plugin::patch::Nop(0x5841C7, 5);
    } else {
        plugin::patch::SetRaw(0x5841B8, s_cheatFirst, sizeof(s_cheatFirst));
        plugin::patch::SetRaw(0x5841C7, s_cheatSecond, sizeof(s_cheatSecond));
    }
}

void SetFastClock(bool enable) {
    if (!s_fastClockCaptured) {
        s_fastClockValue = plugin::patch::Get<bool>(0x95CDBB);
        s_fastClockCaptured = true;
    }
    plugin::patch::Set<bool>(0x95CDBB, enable, false);
}

namespace {
unsigned char NormalizePickupType(unsigned int type) {
    if (type > PICKUP_NUMOFTYPES) return PICKUP_ONCE;
    return static_cast<unsigned char>(type);
}
} // namespace

int SpawnPickupNearPlayer(const Types::PickupOptions& options, float& x, float& y, float& z) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return -1;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<plugin::Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(hplayer, 0.0f, 2.0f, 0.0f, &x, &y, &z);
    z += 0.2f;
    return SpawnPickupAt(options, x, y, z);
}

int SpawnPickupAt(const Types::PickupOptions& options, float x, float y, float z) {
    const int model = static_cast<int>(options.modelId);
    if (model <= 0) return -1;
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    const CVector pos(x, y, z);
    const int handle = CPickups::GenerateNewOne(
        pos,
        static_cast<unsigned int>(model),
        NormalizePickupType(options.type),
        options.quantity
    );
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    return handle;
}

void RemovePickup(int handle) {
    if (handle < 0) return;
    CPickups::RemovePickUp(handle);
}

} // namespace XBase::Detail::WorldBackend