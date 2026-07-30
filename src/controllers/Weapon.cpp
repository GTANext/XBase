#include <XBase/Weapon.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CPickups.h"
#include "CCheat.h"
#include "CCarCtrl.h"
#include "extensions/ScriptCommands.h"

namespace XBase::Weapon {

namespace {
bool s_infiniteAmmo = false;
bool s_fastReload = false;
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
}

void SetInfiniteAmmo(bool enable) {
    s_infiniteAmmo = enable;
    *reinterpret_cast<bool*>(0x969178) = enable;
}

void SetFastReload(bool enable) {
    s_fastReload = enable;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    plugin::Command<plugin::Commands::SET_PLAYER_FAST_RELOAD>(CPools::GetPedRef(player), enable);
}

void GiveAll() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const unsigned int weapons[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46
    };
    for (unsigned int w : weapons) {
        Give(w, 99999);
    }
}

void ClearAll() {
    CPlayerPed* player = FindPlayerPed();
    if (player) player->ClearWeapons();
}

void DropWeapon() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);
    float x, y, z;
    plugin::Command<plugin::Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(hplayer, 0.0f, 3.0f, 0.0f, &x, &y, &z);

    const eWeaponType weaponType = player->m_aWeapons[player->m_nSelectedWepSlot].m_eWeaponType;
    if (weaponType == WEAPONTYPE_UNARMED) return;

    int model = 0, pickup = 0;
    plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(weaponType, &model);
    plugin::Command<plugin::Commands::CREATE_PICKUP_WITH_AMMO>(model, 3, 999, x, y, z, &pickup);
    plugin::Command<plugin::Commands::REMOVE_WEAPON_FROM_CHAR>(hplayer, weaponType);
}

void DropCurrent() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<plugin::Commands::REMOVE_WEAPON_FROM_CHAR>(
        hplayer, player->m_aWeapons[player->m_nSelectedWepSlot].m_eWeaponType);
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

void Give(unsigned int weaponType, unsigned int ammo) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);

    int model = 0;
    plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(weaponType, &model);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(hplayer, weaponType, ammo);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
}

void GiveModel(unsigned int weaponModel, unsigned int ammo) {
    const int model = static_cast<int>(weaponModel);
    if (model <= 0) return;

    eWeaponType weaponType = WEAPONTYPE_UNARMED;
    for (int t = 0; t <= 46; ++t) {
        int checkModel = -1;
        plugin::Command<plugin::Commands::GET_WEAPONTYPE_MODEL>(t, &checkModel);
        if (checkModel == model) {
            weaponType = static_cast<eWeaponType>(t);
            break;
        }
    }
    if (weaponType == WEAPONTYPE_UNARMED) return;

    Give(static_cast<unsigned int>(weaponType), ammo);
    plugin::Command<plugin::Commands::SET_CURRENT_CHAR_WEAPON>(CPools::GetPedRef(FindPlayerPed()), weaponType);
}

void MaxWeaponSkills() {
    CCheat::WeaponSkillsCheat();
}

void ResetStats() {
    plugin::Command<plugin::Commands::SET_FLOAT_STAT>(71, 0.0f);
}

} // namespace XBase::Weapon
