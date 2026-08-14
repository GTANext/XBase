#include "WeaponBackend.h"

#include "CPlayerPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CPickups.h"
#include "CWeaponInfo.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

#include <cmath>
#include <vector>

namespace XBase::Detail::WeaponBackend {

namespace {
bool s_infiniteAmmo = false;
bool s_fastReload = false;
Weapon::StatOverrides s_overrides;
bool s_wasAny = false;
int s_lastType = -1;
unsigned char s_lastMask = 0;
int s_lastRateQ = 100;
std::vector<int> s_trackedPickups;

CPlayerPed* GetPlayer() { return FindPlayerPed(); }

int GetWeaponModel(eWeaponType weaponType) {
    return plugin::CallAndReturnDynGlobal<int, int>(0x4418B0, static_cast<int>(weaponType));
}

eWeaponType GetWeaponTypeFromModel(int model) {
    for (int i = 0; i <= 39; ++i) {
        const auto weaponType = static_cast<eWeaponType>(i);
        if (GetWeaponModel(weaponType) == model) return weaponType;
    }
    return WEAPONTYPE_UNARMED;
}

void ClearPlayerWeapon(CPlayerPed* player, eWeaponType weaponType) {
    if (!player) return;
    const CWeaponInfo* weaponInfo = CWeaponInfo::GetWeaponInfo(weaponType);
    if (!weaponInfo) return;
    const int weaponSlot = static_cast<int>(weaponInfo->m_WeaponSlot);
    if (weaponSlot == -1) return;
    CWeapon* weapon = &player->m_aWeapons[weaponSlot];
    if (weapon->m_eWeaponType != weaponType) return;
    if (player->m_nCurrentWeapon == weaponSlot) {
        CWeaponInfo* unarmedInfo = CWeaponInfo::GetWeaponInfo(WEAPONTYPE_UNARMED);
        if (unarmedInfo) {
            player->SetCurrentWeapon(static_cast<int>(unarmedInfo->m_WeaponSlot));
        }
    }
    weapon->Shutdown();
}

void ScaleAnimLoop(float& start, float& end, float& fire, float rate) {
    if (rate < 0.1f) rate = 0.1f;
    if (rate > 20.0f) rate = 20.0f;
    if (std::fabs(rate - 1.0f) < 0.001f || end <= start) return;
    const float oldStart = start;
    const float oldEnd = end;
    const float oldFire = fire;
    end = oldStart + (oldEnd - oldStart) / rate;
    fire = oldStart + (oldFire - oldStart) / rate;
    if (fire < start) fire = start;
    if (fire > end) fire = end;
}

bool AnyOverrideActive() {
    const float rate = (s_overrides.customFireRate && s_overrides.fireRate > 0.1f) ? s_overrides.fireRate : 1.0f;
    const bool fireRateActive = s_overrides.customFireRate && std::fabs(rate - 1.0f) >= 0.001f;
    return s_overrides.hugeDamage || s_overrides.longRange || s_overrides.noSpread || fireRateActive;
}

void ProcessStatOverrides(CPlayerPed* player) {
    const float rate = (s_overrides.customFireRate && s_overrides.fireRate > 0.1f) ? s_overrides.fireRate : 1.0f;
    const bool fireRateActive = s_overrides.customFireRate && std::fabs(rate - 1.0f) >= 0.001f;

    const unsigned char mask =
        static_cast<unsigned char>((s_overrides.hugeDamage ? 1 : 0) |
                                   (s_overrides.longRange ? 2 : 0) |
                                   (s_overrides.noSpread ? 4 : 0) |
                                   (fireRateActive ? 8 : 0));
    const int rateQ = fireRateActive ? static_cast<int>(rate * 100.0f + 0.5f) : 100;

    if (!AnyOverrideActive()) {
        if (s_wasAny) {
            CWeaponInfo::LoadWeaponData();
            s_wasAny = false;
            s_lastType = -1;
            s_lastMask = 0;
            s_lastRateQ = 100;
        }
        return;
    }
    if (!player) return;

    CWeapon& weapon = player->m_aWeapons[player->m_nCurrentWeapon];
    const int type = static_cast<int>(weapon.m_eWeaponType);
    if (s_wasAny && s_lastType == type && s_lastMask == mask && s_lastRateQ == rateQ) return;
    if (s_wasAny) {
        CWeaponInfo::LoadWeaponData();
    }

    CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(weapon.m_eWeaponType);
    if (!info) return;

    if (s_overrides.hugeDamage) info->m_nDamage = 1000;
    if (s_overrides.longRange) info->m_fRange = 1000.0f;
    if (s_overrides.noSpread) info->m_fSpread = 0.0f;
    if (fireRateActive) {
        if (info->m_nFiringRate > 0) {
            const float scaled = static_cast<float>(info->m_nFiringRate) / rate;
            info->m_nFiringRate = scaled < 1.0f ? 1u : static_cast<unsigned int>(scaled);
        }
        ScaleAnimLoop(info->m_fAnimLoopStart, info->m_fAnimLoopEnd, info->m_fAnimFrameFire, rate);
        ScaleAnimLoop(info->m_fAnim2LoopStart, info->m_fAnim2LoopEnd, info->m_fAnim2FrameFire, rate);
    }

    s_wasAny = true;
    s_lastType = type;
    s_lastMask = mask;
    s_lastRateQ = rateQ;
}
} // namespace

void Init() {
    s_infiniteAmmo = false;
    s_fastReload = false;
    s_overrides = Weapon::StatOverrides{};
    s_wasAny = false;
    s_lastType = -1;
    s_lastMask = 0;
    s_lastRateQ = 100;
    s_trackedPickups.clear();
}

void Shutdown() {
    SetInfiniteAmmo(false);
    SetFastReload(false);
    s_overrides = Weapon::StatOverrides{};
    if (s_wasAny) {
        CWeaponInfo::LoadWeaponData();
        s_wasAny = false;
    }
}

void Process() {
    CPlayerPed* player = GetPlayer();
    if (!player) return;

    if (s_infiniteAmmo) {
        for (int i = 0; i < 10; i++) {
            player->m_aWeapons[i].m_nAmmoTotal = 9999;
        }
    }
    if (s_fastReload) {
        plugin::Command<plugin::Commands::SET_PLAYER_FAST_RELOAD>(CPools::GetPedRef(player), true);
    }
    ProcessStatOverrides(player);
}

bool Give(unsigned int weaponType, unsigned int ammo) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    const int model = GetWeaponModel(static_cast<eWeaponType>(weaponType));
    if (model <= 0) return false;
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(hplayer, static_cast<eWeaponType>(weaponType), ammo);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    return true;
}

bool GiveModel(unsigned int weaponModel, unsigned int ammo) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const int model = static_cast<int>(weaponModel);
    if (model <= 0) return false;
    const int hplayer = CPools::GetPedRef(player);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    const eWeaponType weaponType = GetWeaponTypeFromModel(model);
    if (weaponType == WEAPONTYPE_UNARMED) {
        plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
        return false;
    }
    plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(hplayer, weaponType, ammo);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    return true;
}

bool GiveAll() {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const unsigned int weapons[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
    };
    bool success = true;
    for (const unsigned int weapon : weapons) {
        success = Give(weapon, 99999) && success;
    }
    return success;
}

bool ClearAll() {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    player->ClearWeapons();
    return true;
}

bool DropWeapon() {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    plugin::Command<plugin::Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(hplayer, 0.0f, 3.0f, 0.0f, &x, &y, &z);

    const eWeaponType weaponType = player->m_aWeapons[player->m_nCurrentWeapon].m_eWeaponType;
    if (weaponType == WEAPONTYPE_UNARMED) return false;

    const int model = GetWeaponModel(weaponType);
    int pickup = 0;
    plugin::Command<plugin::Commands::CREATE_PICKUP_WITH_AMMO>(model, 3, 999, x, y, z, &pickup);
    if (pickup > 0) s_trackedPickups.push_back(pickup);
    ClearPlayerWeapon(player, weaponType);
    return true;
}

bool DropCurrent() {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    ClearPlayerWeapon(player, player->m_aWeapons[player->m_nCurrentWeapon].m_eWeaponType);
    return true;
}

bool MaxWeaponSkills() { return false; }

bool ResetStats() {
    CWeaponInfo::LoadWeaponData();
    s_wasAny = false;
    return true;
}

void SetInfiniteAmmo(bool enable) {
    s_infiniteAmmo = enable;
}

void SetFastReload(bool enable) {
    s_fastReload = enable;
}

void SetStatOverrides(const Weapon::StatOverrides& overrides) {
    s_overrides = overrides;
    if (!AnyOverrideActive()) {
        s_lastType = -1;
        s_lastMask = 0;
        s_lastRateQ = 100;
    }
}

int RemoveTrackedPickups() {
    int removed = 0;
    for (const int handle : s_trackedPickups) {
        if (handle < 0) continue;
        CPickups::RemovePickUp(handle);
        ++removed;
    }
    s_trackedPickups.clear();
    return removed;
}

} // namespace XBase::Detail::WeaponBackend