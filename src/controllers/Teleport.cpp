#include <XBase/Teleport.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CWorld.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CMenuManager.h"
#include "CRadar.h"
#include "extensions/ScriptCommands.h"
#include <cmath>

namespace XBase::Teleport {

bool TryGetCurrentPosition(Vec3& position) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) {
        position = {};
        return false;
    }
    const CVector& value = player->GetPosition();
    position = {value.x, value.y, value.z};
    return true;
}

Vec3 GetCurrentPosition() {
    Vec3 position{};
    TryGetCurrentPosition(position);
    return position;
}

bool To(float x, float y, float z, int interior) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    CVector pos(x, y, z);
    CVehicle* veh = player->m_pVehicle;
    const int hplayer = CPools::GetPedRef(player);
    const bool jetpack = plugin::Command<plugin::Commands::IS_PLAYER_USING_JETPACK>(0);
    CStreaming::LoadScene(&pos);
    CStreaming::LoadSceneCollision(&pos);
    CStreaming::LoadAllRequestedModels(false);
    if (veh && player->bInVehicle) {
        veh->Teleport(pos, false);
    } else {
        player->Teleport(pos, false);
    }
    if (jetpack) plugin::Command<plugin::Commands::TASK_JETPACK>(hplayer);
    player->m_nAreaCode = interior;
    plugin::Command<plugin::Commands::SET_AREA_VISIBLE>(interior);
    return true;
}

bool Forward(float distance) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const float angle = player->m_fHeadingCurrent;
    const float dx = -std::sin(angle) * distance;
    const float dy = std::cos(angle) * distance;
    CVector pos = player->GetPosition();
    pos.x += dx;
    pos.y += dy;
    return To(pos.x, pos.y, pos.z);
}

bool MapPosition(float x, float y, bool spawnUnderwater) {
    CVector pos(x, y, 0.0f);
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    CStreaming::LoadScene(&pos);
    CStreaming::LoadSceneCollision(&pos);
    CStreaming::LoadAllRequestedModels(false);
    float water = 0.0f;
    CEntity* entity = FindPlayerEntity(-1);
    float ground = CWorld::FindGroundZFor3DCoord(pos.x, pos.y, 1000.0f, nullptr, &entity) + 1.0f;
    if (!spawnUnderwater) {
        plugin::Command<plugin::Commands::GET_WATER_HEIGHT_AT_COORDS>(pos.x, pos.y, true, &water);
        pos.z = ground > water ? ground : water;
    } else {
        pos.z = ground;
    }
    return To(pos.x, pos.y, pos.z);
}

bool Marker(bool spawnUnderwater) {
    const auto index = static_cast<unsigned short>(FrontEndMenuManager.m_nTargetBlipIndex);
    const tRadarTrace& blip = CRadar::ms_RadarTrace[index];
    if (blip.m_nRadarSprite != RADAR_SPRITE_WAYPOINT) return false;
    return MapPosition(blip.m_vecPos.x, blip.m_vecPos.y, spawnUnderwater);
}

bool Center() {
    return To(0.0f, 0.0f, 3.0f, 0);
}

void Process() {
}

} // namespace XBase::Teleport
