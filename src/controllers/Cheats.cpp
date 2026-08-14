#include <XBase/Cheats.h>

#include "plugin.h"
#include "CTimer.h"
#include "CRGBA.h"
#include "CRect.h"
#include "CSprite2d.h"
#include "RenderWare.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CPools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace {

constexpr std::array<const char*, 92> kRandomCheatNames = {{
    "Weapon Set 1", "Weapon Set 2", "Weapon Set 3", "Health Armor 250k",
    "Wanted Level 2 Stars", "Clear Wanted Level", "Sunny Weather", "Very Sunny Weather",
    "Overcast Weather", "Rainy Weather", "Foggy Weather", "Faster Clock",
    "Faster Gameplay", "Slower Gameplay", "Peds Attack Other With Golfclub",
    "Have A Bounty On Your Head", "Everyone Armed", "Spawn Rhino", "Spawn Bloodring Banger",
    "Spawn Rancher", "Spawn HotringA", "Spawn HotringB", "Spawn Romero", "Spawn Stretch",
    "Spawn Trashmaster", "Spawn Caddy", "Blow Up All Cars", "Invisible Car", "Perfect Handling",
    "Suicide", "Green Lights Cheat", "Aggressive Drivers", "Pink Traffic", "Black Traffic",
    "Cars On Water", "Boats Fly", "Fat Player", "Max Muscle", "Skinny Player",
    "Elvis Is Everywhere", "Peds Attack You With Rockets", "Beach Party", "Gang Members Everywhere",
    "Gangs Controls The Streets", "Ninja Theme", "Slut Magnet", "Cheap Traffic", "Fast Traffic",
    "Cars Fly", "Huge Bunny Hop", "Spawn Hydra", "Spawn Vortex", "Smash N Boom",
    "All Cars Have Nitro", "Cars Float Away When Hit", "Always Midnight", "Stop Game Clock Orange Sky",
    "Thunder Storm", "Sand Storm", "Unused", "Mega Jump", "Infinite Health", "Infinite Oxygen",
    "Get Parachute", "Get Jetpack", "I Do As I Please", "Six Wanted Stars", "Mega Punch",
    "Never Get Hungry", "Riot Mode", "Funhouse Theme", "Adrenaline Mode", "Infinite Ammo",
    "Weapon Aiming While Driving", "Reduced Traffic", "Country Traffic", "Wanna Be In My Gang",
    "No One Can Stop Us", "Rocket Mayhem", "Max Respect", "Max Sex Appeal", "Max Stamina",
    "Hitman Level For All Weapons", "Max Driving Skills", "Spawn Hunter", "Spawn Quad",
    "Spawn Tanker Truck", "Spawn Dozer", "Spawn Stunt Plane", "Spawn Monster",
    "Prostitutes Pay You", "All Taxis Nitro"
}};

std::array<bool, kRandomCheatNames.size()> MakeDefaultEnabledState() {
    std::array<bool, kRandomCheatNames.size()> enabled{};
    enabled.fill(true);
    return enabled;
}

std::array<bool, kRandomCheatNames.size()> s_enabled = MakeDefaultEnabledState();
XBase::Cheats::RandomSettings s_settings;
unsigned int s_lastExecutionAt = 0;
bool s_drawSubscribed = false;

bool s_boatFly = false;
bool s_driveWater = false;
bool s_tankMode = false;
bool s_aimDrive = false;
bool s_noDerail = false;
bool s_flipNoBurn = false;
bool s_stayOnBike = false;
bool s_bikeFly = false;

unsigned char s_savedKnockOff = 0;
CPlayerPed* s_knockOffPlayer = nullptr;
bool s_hasSavedKnockOff = false;

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

PatchSnapshot s_noDerailA{0x6F8C2A, {}, 4, false};
PatchSnapshot s_noDerailB{0x6F8C2E, {}, 1, false};
PatchSnapshot s_noDerailC{0x6F8C41, {}, 2, false};

PatchSnapshot s_flipA{0x6A776B, {}, 6, false};
PatchSnapshot s_flipB{0x570E7F, {}, 6, false};

constexpr std::array<unsigned char, 6> kDisableBurn = {0xD8, 0xDD, 0x00, 0x00, 0x00, 0x00};

void ApplyNoDerail(bool enable) {
    if (enable) {
        s_noDerailA.Capture();
        s_noDerailB.Capture();
        s_noDerailC.Capture();
        plugin::patch::Set<uint32_t>(0x6F8C2A, 0x00441F0F, true);
        plugin::patch::Set<uint8_t>(0x6F8C2E, 0x00, true);
        plugin::patch::Set<uint16_t>(0x6F8C41, 0xE990, true);
    } else {
        s_noDerailA.Restore();
        s_noDerailB.Restore();
        s_noDerailC.Restore();
    }
}

void ApplyFlipNoBurn(bool enable) {
    if (enable) {
        s_flipA.Capture();
        s_flipB.Capture();
        std::array<unsigned char, 6> burn = kDisableBurn;
        plugin::patch::SetRaw(s_flipA.address, burn.data(), burn.size(), true);
        plugin::patch::SetRaw(s_flipB.address, burn.data(), burn.size(), true);
    } else {
        s_flipA.Restore();
        s_flipB.Restore();
    }
}

bool IsPedPointerInPool(CPed* ped) {
    if (!ped) return false;
    if (!CPools::ms_pPedPool) return false;
    for (CPed* pooledPed : *CPools::ms_pPedPool) {
        if (pooledPed == ped) return true;
    }
    return false;
}

void DrawRandomCheatProgress() {
    if (!s_settings.enabled || !s_settings.showProgress) return;

    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    const float elapsed = static_cast<float>(now - s_lastExecutionAt) / 1000.0f;
    const float progress = std::clamp(
        (static_cast<float>(s_settings.intervalSeconds) - elapsed)
            / static_cast<float>(s_settings.intervalSeconds),
        0.0f, 1.0f);
    const float width = static_cast<float>(RsGlobal.maximumWidth);
    const float height = static_cast<float>(RsGlobal.maximumHeight) / 50.0f;
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, width, height), CRGBA(24, 99, 44, 255));
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, width * progress, height), CRGBA(33, 145, 63, 255));
}

} // namespace

namespace XBase::Cheats {

void FlyingCars(bool enable) {
    *reinterpret_cast<bool*>(0x969130 + 48) = enable;
}

bool IsFlyingCars() {
    return *reinterpret_cast<bool*>(0x969130 + 48);
}

void AllCarsHaveNitro(bool enable) {
    *reinterpret_cast<bool*>(0x969130 + 53) = enable;
}

bool IsAllCarsHaveNitro() {
    return *reinterpret_cast<bool*>(0x969130 + 53);
}

void PerfectHandling(bool enable) {
    *reinterpret_cast<bool*>(0x969130 + 28) = enable;
}

bool IsPerfectHandling() {
    return *reinterpret_cast<bool*>(0x969130 + 28);
}

void GreenLights(bool enable) {
    *reinterpret_cast<bool*>(0x969130 + 30) = enable;
}

bool IsGreenLights() {
    return *reinterpret_cast<bool*>(0x969130 + 30);
}

void Riot(bool enable) {
    *reinterpret_cast<bool*>(0x969130 + 69) = enable;
}

bool IsRiot() {
    return *reinterpret_cast<bool*>(0x969130 + 69);
}

void BoatFly(bool enable) {
    s_boatFly = enable;
    *reinterpret_cast<bool*>(0x969130 + 35) = enable;
}

bool IsBoatFly() {
    return s_boatFly;
}

void DriveWater(bool enable) {
    s_driveWater = enable;
    *reinterpret_cast<bool*>(0x969130 + 34) = enable;
}

bool IsDriveWater() {
    return s_driveWater;
}

void TankMode(bool enable) {
    s_tankMode = enable;
    *reinterpret_cast<bool*>(0x969130 + 52) = enable;
}

bool IsTankMode() {
    return s_tankMode;
}

void AimDrive(bool enable) {
    s_aimDrive = enable;
    *reinterpret_cast<bool*>(0x969130 + 73) = enable;
}

bool IsAimDrive() {
    return s_aimDrive;
}

void NoDerail(bool enable) {
    if (s_noDerail == enable) return;
    s_noDerail = enable;
    ApplyNoDerail(enable);
}

bool IsNoDerail() {
    return s_noDerail;
}

void FlipNoBurn(bool enable) {
    if (s_flipNoBurn == enable) return;
    s_flipNoBurn = enable;
    ApplyFlipNoBurn(enable);
}

bool IsFlipNoBurn() {
    return s_flipNoBurn;
}

void StayOnBike(bool enable) {
    s_stayOnBike = enable;
}

bool IsStayOnBike() {
    return s_stayOnBike;
}

void BikeFly(bool enable) {
    s_bikeFly = enable;
}

bool IsBikeFly() {
    return s_bikeFly;
}

void SetRandomSettings(const RandomSettings& settings) {
    s_settings.enabled = settings.enabled;
    s_settings.showProgress = settings.showProgress;
    s_settings.intervalSeconds = std::clamp(settings.intervalSeconds, 1, 60);
}

RandomSettings GetRandomSettings() {
    return s_settings;
}

std::size_t GetRandomCheatCount() {
    return kRandomCheatNames.size();
}

const char* GetRandomCheatName(std::size_t index) {
    return index < kRandomCheatNames.size() ? kRandomCheatNames[index] : nullptr;
}

bool IsRandomCheatEnabled(std::size_t index) {
    return index < s_enabled.size() && s_enabled[index];
}

bool SetRandomCheatEnabled(std::size_t index, bool enabled) {
    if (index >= s_enabled.size()) return false;
    s_enabled[index] = enabled;
    return true;
}

void Init() {
    if (s_drawSubscribed) return;
    plugin::Events::drawingEvent += DrawRandomCheatProgress;
    s_drawSubscribed = true;
}

void NotifyGameInit() {
    s_lastExecutionAt = CTimer::m_snTimeInMilliseconds;
    s_hasSavedKnockOff = false;
    s_knockOffPlayer = nullptr;
}

void Process() {
    CPlayerPed* player = FindPlayerPed();
    if (player && IsPedPointerInPool(player)) {
        if (s_stayOnBike) {
            if (!s_hasSavedKnockOff || s_knockOffPlayer != player) {
                s_savedKnockOff = player->CantBeKnockedOffBike;
                s_knockOffPlayer = player;
                s_hasSavedKnockOff = true;
            }
            player->CantBeKnockedOffBike = 1;
        } else if (s_hasSavedKnockOff && s_knockOffPlayer == player) {
            player->CantBeKnockedOffBike = s_savedKnockOff;
            s_hasSavedKnockOff = false;
            s_knockOffPlayer = nullptr;
        }
    } else {
        s_hasSavedKnockOff = false;
        s_knockOffPlayer = nullptr;
    }

    if (s_bikeFly && player) {
        CVehicle* vehicle = player->m_pVehicle;
        if (vehicle && vehicle->IsDriver(player) &&
            (vehicle->m_nVehicleSubClass == VEHICLE_BIKE || vehicle->m_nVehicleSubClass == VEHICLE_BMX)) {
            const float speed = std::sqrt(
                vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x +
                vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y +
                vehicle->m_vecMoveSpeed.z * vehicle->m_vecMoveSpeed.z);
            if (speed > 0.0f && CTimer::ms_fTimeStep > 0.0f) {
                vehicle->FlyingControl(3, -9999.9902f, -9999.9902f, -9999.9902f, -9999.9902f);
            }
        }
    }

    if (!s_settings.enabled) return;

    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    const unsigned int interval = static_cast<unsigned int>(s_settings.intervalSeconds) * 1000u;
    if (now - s_lastExecutionAt >= interval) {
        std::array<int, kRandomCheatNames.size()> candidates{};
        std::size_t candidateCount = 0;
        for (std::size_t index = 0; index < s_enabled.size(); ++index) {
            if (s_enabled[index]) candidates[candidateCount++] = static_cast<int>(index);
        }

        if (candidateCount > 0) {
            const int candidateIndex = plugin::RandomNumberInRange(0, static_cast<int>(candidateCount - 1));
            const int id = candidates[static_cast<std::size_t>(candidateIndex)];
            plugin::Call<0x438370>(id);
            plugin::Call<0x69F0B0>(const_cast<char*>(kRandomCheatNames[static_cast<std::size_t>(id)]), 2000, 0, false);
        }
        s_lastExecutionAt = now;
    }
}

void Shutdown() {
    s_settings.enabled = false;
    s_lastExecutionAt = 0;
    if (s_drawSubscribed) {
        plugin::Events::drawingEvent -= DrawRandomCheatProgress;
        s_drawSubscribed = false;
    }
    s_boatFly = false;
    s_driveWater = false;
    s_tankMode = false;
    s_aimDrive = false;
    s_bikeFly = false;
    s_stayOnBike = false;
    ApplyNoDerail(false);
    ApplyFlipNoBurn(false);
    s_noDerail = false;
    s_flipNoBurn = false;
    if (s_hasSavedKnockOff) {
        if (s_knockOffPlayer) s_knockOffPlayer->CantBeKnockedOffBike = s_savedKnockOff;
        s_hasSavedKnockOff = false;
        s_knockOffPlayer = nullptr;
    }
}

} // namespace XBase::Cheats