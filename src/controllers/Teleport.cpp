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

namespace XBase::Teleport {

CVector GetCurrentPosition() {
    CPlayerPed* player = FindPlayerPed();
    return player ? player->GetPosition() : CVector(0.0f, 0.0f, 10.0f);
}

    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
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
}

void Forward(float distance) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const float angle = player->m_fHeadingCurrent;
    const float dx = -std::sin(angle) * distance;
    const float dy = std::cos(angle) * distance;
    CVector pos = player->GetPosition();
    pos.x += dx;
    pos.y += dy;
    To(pos.x, pos.y, pos.z);
}

void MapPosition(float x, float y, bool spawnUnderwater) {
    CVector pos(x, y, 0.0f);
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
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
    To(pos.x, pos.y, pos.z);
}

void Marker(bool spawnUnderwater) {
    const auto index = static_cast<unsigned short>(FrontEndMenuManager.m_nTargetBlipIndex);
    const tRadarTrace& blip = CRadar::ms_RadarTrace[index];
    if (blip.m_nRadarSprite != RADAR_SPRITE_WAYPOINT) return;
    MapPosition(blip.m_vecPos.x, blip.m_vecPos.y, spawnUnderwater);
}

void Center() {
    To(0.0f, 0.0f, 3.0f, 0);
}

void Process() {
}

} // namespace XBase::Teleport
