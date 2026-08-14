#include <XBase/Weapon.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CWorld.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CPickups.h"
#include "CCheat.h"
#include "CCarCtrl.h"
#include "CWeaponInfo.h"
#include "CCamera.h"
#include "CPad.h"
#include "extensions/ScriptCommands.h"

#include <cmath>
#include <Windows.h>

namespace XBase::Weapon {

namespace {
bool s_infiniteAmmo = false;
bool s_fastReload = false;
bool s_hasInfiniteAmmoSnapshot = false;
bool s_savedInfiniteAmmo = false;
bool s_hasFastReloadSnapshot = false;
bool s_savedFastReload = false;

StatOverrides s_overrides;
bool s_wasAny = false;
int s_lastType = -1;
unsigned char s_lastSkill = 255;
unsigned char s_lastMask = 0;
int s_lastRateQ = 100;

bool s_savedMouse3rd = true;
bool s_hasMouseSaved = false;
bool s_autoAimWasOn = false;

void ProcessAutoAim(bool enable) {
    if (!enable) {
        if (s_autoAimWasOn && s_hasMouseSaved) {
            CCamera::m_bUseMouse3rdPerson = s_savedMouse3rd;
        }
        s_autoAimWasOn = false;
        s_hasMouseSaved = false;
        return;
    }

    if (!s_autoAimWasOn) {
        s_savedMouse3rd = CCamera::m_bUseMouse3rdPerson;
        s_hasMouseSaved = true;
        s_autoAimWasOn = true;
    }

    if (CPad::NewMouseControllerState.x == 0 && CPad::NewMouseControllerState.y == 0) {
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
            CCamera::m_bUseMouse3rdPerson = false;
        }
    } else {
        CCamera::m_bUseMouse3rdPerson = true;
    }
}

CPlayerInfo* GetPlayerInfo() {
    return CWorld::Players ? &CWorld::Players[CWorld::PlayerInFocus] : nullptr;
}

void ScaleAnimLoop(float& start, float& end, float& fire, float rate) {
    if (rate < 0.1f) rate = 0.1f;
    if (rate > 20.0f) rate = 20.0f;
    if (std::fabs(rate - 1.0f) < 0.001f || end <= start) return;
    const float oldStart = start;
    const float oldEnd = end;
    const float oldFire = fire;
    const float newLen = (oldEnd - oldStart) / rate;
    end = oldStart + newLen;
    fire = oldStart + (oldFire - oldStart) / rate;
    if (fire < start) fire = start;
    if (fire > end) fire = end;
}

bool AnyOverrideActive() {
    const float rate = (s_overrides.customFireRate && s_overrides.fireRate > 0.1f) ? s_overrides.fireRate : 1.0f;
    const bool fireRateActive = s_overrides.customFireRate && std::fabs(rate - 1.0f) >= 0.001f;
    return s_overrides.hugeDamage || s_overrides.longRange || s_overrides.rapidFire ||
           s_overrides.dualWield || s_overrides.moveAim || s_overrides.moveFire ||
           s_overrides.noSpread || fireRateActive;
}

void ProcessStatOverrides(CPlayerPed* player) {
    const float rate = (s_overrides.customFireRate && s_overrides.fireRate > 0.1f) ? s_overrides.fireRate : 1.0f;
    const bool fireRateActive = s_overrides.customFireRate && std::fabs(rate - 1.0f) >= 0.001f;

    const unsigned char mask =
        static_cast<unsigned char>((s_overrides.hugeDamage ? 1 : 0) |
                                   (s_overrides.longRange ? 2 : 0) |
                                   (s_overrides.rapidFire ? 4 : 0) |
                                   (s_overrides.dualWield ? 8 : 0) |
                                   (s_overrides.moveAim ? 16 : 0) |
                                   (s_overrides.moveFire ? 32 : 0) |
                                   (s_overrides.noSpread ? 64 : 0) |
                                   (fireRateActive ? 128 : 0));
    const int rateQ = fireRateActive ? static_cast<int>(rate * 100.0f + 0.5f) : 100;

    if (!AnyOverrideActive()) {
        if (s_wasAny) {
            CWeaponInfo::LoadWeaponData();
            s_wasAny = false;
            s_lastType = -1;
            s_lastSkill = 255;
            s_lastMask = 0;
            s_lastRateQ = 100;
        }
        return;
    }
    if (!player) return;

    CWeapon& weapon = player->m_aWeapons[player->m_nSelectedWepSlot];
    const unsigned char skill = player->GetWeaponSkill(weapon.m_eWeaponType);
    const int type = static_cast<int>(weapon.m_eWeaponType);

    if (s_wasAny && s_lastType == type && s_lastSkill == skill && s_lastMask == mask && s_lastRateQ == rateQ) {
        return;
    }

    if (s_wasAny) {
        CWeaponInfo::LoadWeaponData();
    }

    CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(weapon.m_eWeaponType, skill);
    if (!info) return;

    if (s_overrides.hugeDamage) {
        info->m_nDamage = 1000;
    }
    if (s_overrides.longRange) {
        info->m_fTargetRange = 1000.0f;
        info->m_fWeaponRange = 1000.0f;
        info->m_fAccuracy = 1.0f;
        info->m_nFlags.bReload2Start = true;
    }
    if (s_overrides.rapidFire && weapon.m_eWeaponType != WEAPONTYPE_FTHROWER && weapon.m_eWeaponType != WEAPONTYPE_MINIGUN) {
        info->m_nFlags.bContinuosFire = true;
    }
    if (s_overrides.dualWield && (weapon.m_eWeaponType == WEAPONTYPE_PISTOL || weapon.m_eWeaponType == WEAPONTYPE_MICRO_UZI || weapon.m_eWeaponType == WEAPONTYPE_TEC9 || weapon.m_eWeaponType == WEAPONTYPE_SAWNOFF)) {
        info->m_nFlags.bTwinPistol = true;
    }
    if (s_overrides.moveAim) {
        info->m_nFlags.bMoveAim = true;
    }
    if (s_overrides.moveFire) {
        info->m_nFlags.bMoveFire = true;
    }
    if (s_overrides.noSpread) {
        info->m_fAccuracy = 100.0f;
    }
    if (fireRateActive) {
        ScaleAnimLoop(info->m_fAnimLoopStart, info->m_fAnimLoopEnd, *reinterpret_cast<float*>(&info->m_nAnimLoopFire), rate);
        ScaleAnimLoop(*reinterpret_cast<float*>(&info->m_nAnimLoop2Start), *reinterpret_cast<float*>(&info->m_nAnimLoop2End), *reinterpret_cast<float*>(&info->m_nAnimLoop2Fire), rate);
    }

    s_wasAny = true;
    s_lastType = type;
    s_lastSkill = skill;
    s_lastMask = mask;
    s_lastRateQ = rateQ;
}
}

void NotifyGameInit() {
    s_infiniteAmmo = false;
    s_fastReload = false;
    s_hasInfiniteAmmoSnapshot = false;
    s_hasFastReloadSnapshot = false;
}

void Shutdown() {
    SetInfiniteAmmo(false);
    SetFastReload(false);
    ProcessAutoAim(false);
    if (s_wasAny) {
        CWeaponInfo::LoadWeaponData();
        s_wasAny = false;
    }
    s_overrides = StatOverrides{};
}

void Process() {
    if (!Core::IsWorldReady()) return;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    if (s_infiniteAmmo) {
        *reinterpret_cast<bool*>(0x969178) = true;
    }
    if (s_fastReload) {
        plugin::Command<plugin::Commands::SET_PLAYER_FAST_RELOAD>(CPools::GetPedRef(player), true);
    }

    ProcessStatOverrides(player);
    ProcessAutoAim(s_overrides.autoAim);
}

bool SetInfiniteAmmo(bool enable) {
    if (s_infiniteAmmo == enable) return true;
    bool* value = reinterpret_cast<bool*>(0x969178);
    if (enable) {
        s_savedInfiniteAmmo = *value;
        s_hasInfiniteAmmoSnapshot = true;
        *value = true;
        s_infiniteAmmo = true;
        return true;
    }

    s_infiniteAmmo = false;
    if (s_hasInfiniteAmmoSnapshot) {
        *value = s_savedInfiniteAmmo;
        s_hasInfiniteAmmoSnapshot = false;
    }
    return true;
}

bool SetFastReload(bool enable) {
    if (s_fastReload == enable) return true;
    CPlayerInfo* info = GetPlayerInfo();
    CPlayerPed* player = FindPlayerPed();
    if (enable) {
        if (!info || !player) return false;
        s_savedFastReload = info->m_bFastReload;
        s_hasFastReloadSnapshot = true;
        plugin::Command<plugin::Commands::SET_PLAYER_FAST_RELOAD>(CPools::GetPedRef(player), true);
        s_fastReload = true;
        return true;
    }

    s_fastReload = false;
    if (s_hasFastReloadSnapshot) {
        if (info) info->m_bFastReload = s_savedFastReload;
        s_hasFastReloadSnapshot = false;
    }
    return true;
}

bool GiveAll() {
    if (!FindPlayerPed()) return false;
    const unsigned int weapons[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46
    };
    bool success = true;
    for (unsigned int w : weapons) {
        success = Give(w, 99999) && success;
    }
    return success;
}

bool ClearAll() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    player->ClearWeapons();
    return true;
}

bool DropWeapon() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    float x, y, z;
    plugin::Command<plugin::Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(hplayer, 0.0f, 3.0f, 0.0f, &x, &y, &z);

    const eWeaponType weaponType = player->m_aWeapons[player->m_nSelectedWepSlot].m_eWeaponType;
    if (weaponType == WEAPONTYPE_UNARMED) return false;

    int model = 0, pickup = 0;
    plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(weaponType, &model);
    plugin::Command<plugin::Commands::CREATE_PICKUP_WITH_AMMO>(model, 3, 999, x, y, z, &pickup);
    plugin::Command<plugin::Commands::REMOVE_WEAPON_FROM_CHAR>(hplayer, weaponType);
    return true;
}

bool DropCurrent() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    const eWeaponType weaponType = player->m_aWeapons[player->m_nSelectedWepSlot].m_eWeaponType;
    if (weaponType == WEAPONTYPE_UNARMED) return false;
    plugin::Command<plugin::Commands::REMOVE_WEAPON_FROM_CHAR>(hplayer, weaponType);
    return true;
}

int RemoveTrackedPickups() {
    int count = 0;
    for (unsigned int i = 0; i < MAX_NUM_PICKUPS; i++) {
        CPickup* p = &CPickups::aPickUps[i];
        if (p && p->m_nFlags.bVisible) {
            CPickups::RemovePickUp(i);
            count++;
        }
    }
    return count;
}

bool Give(unsigned int weaponType, unsigned int ammo) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);

    int model = 0;
    plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(weaponType, &model);
    if (model <= 0) return false;
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(hplayer, weaponType, ammo);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    return true;
}

bool GiveModel(unsigned int weaponModel, unsigned int ammo) {
    const int model = static_cast<int>(weaponModel);
    if (model <= 0) return false;

    eWeaponType weaponType = WEAPONTYPE_UNARMED;
    for (int t = 0; t <= 46; ++t) {
        int checkModel = -1;
        plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(t, &checkModel);
        if (checkModel == model) {
            weaponType = static_cast<eWeaponType>(t);
            break;
        }
    }
    if (weaponType == WEAPONTYPE_UNARMED) return false;

    CPlayerPed* player = FindPlayerPed();
    if (!player || !Give(static_cast<unsigned int>(weaponType), ammo)) return false;
    plugin::Command<plugin::Commands::SET_CURRENT_CHAR_WEAPON>(CPools::GetPedRef(player), weaponType);
    return true;
}

bool MaxWeaponSkills() {
    if (!FindPlayerPed()) return false;
    CCheat::WeaponSkillsCheat();
    return true;
}

bool ResetStats() {
    if (!FindPlayerPed()) return false;
    plugin::Command<plugin::Commands::SET_FLOAT_STAT>(71, 0.0f);
    return true;
}

void SetStatOverrides(const StatOverrides& overrides) {
    s_overrides = overrides;
    if (!AnyOverrideActive()) {
        s_lastType = -1;
        s_lastSkill = 255;
        s_lastMask = 0;
        s_lastRateQ = 100;
    }
}

} // namespace XBase::Weapon
