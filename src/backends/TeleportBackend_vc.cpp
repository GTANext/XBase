#include "TeleportBackend.h"

#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CStreaming.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

#include <cmath>

namespace XBase::Detail::TeleportBackend {

namespace {
CPlayerPed* GetPlayer() {
    return FindPlayerPed();
}
} // namespace

bool GetPlayerPosition(Vec3& position) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const CVector value = player->GetPosition();
    position = { value.x, value.y, value.z };
    return true;
}

bool To(const Vec3& position, int interior) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const CVector pos(position.x, position.y, position.z);
    CVehicle* veh = player->m_pVehicle;

    CStreaming::LoadScene(&pos);
    CStreaming::LoadSceneCollision(&pos);
    CStreaming::LoadAllRequestedModels(false);

    if (veh && player->m_bInVehicle) {
        veh->Teleport(pos);
    } else {
        player->Teleport(pos);
    }
    player->m_nAreaCode = interior;
    plugin::Command<plugin::Commands::SET_AREA_VISIBLE>(interior);
    return true;
}

bool Forward(float distance) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    const float angle = player->m_fHeadingCurrent;
    const CVector value = player->GetPosition();
    const Vec3 target{
        value.x - std::sin(angle) * distance,
        value.y + std::cos(angle) * distance,
        value.z
    };
    return To(target, player->m_nAreaCode);
}

bool MapPosition(const Vec3& position, bool spawnUnderwater) {
    CPlayerPed* player = GetPlayer();
    if (!player) return false;
    float ground = 0.0f;
    if (plugin::Command<plugin::Commands::GET_GROUND_Z_FOR_3D_COORD>(position.x, position.y, 1000.0f, &ground)) {
        const Vec3 target{ position.x, position.y, spawnUnderwater ? ground : ground + 1.0f };
        return To(target, player->m_nAreaCode);
    }
    return To(position, player->m_nAreaCode);
}

bool Marker(bool) { return false; }

bool Center() { return To({ 0.0f, 0.0f, 3.0f }, 0); }

} // namespace XBase::Detail::TeleportBackend