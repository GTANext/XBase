#include "PedBackend.h"

#include "CModelInfo.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CPed.h"
#include "CStreaming.h"
#include "CWorld.h"
#include "CRadar.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::PedBackend {
namespace {
CPed* AsPed(void* ped) { return static_cast<CPed*>(ped); }
}

bool IsValidModel(unsigned int modelId) {
    return CModelInfo::IsPedModel(static_cast<int>(modelId));
}

void* GetPlayer() { return FindPlayerPed(); }
bool IsPlayer(void* ped) { return ped && ped == FindPlayerPed(); }
bool IsValid(void* ped) {
    if (!ped || !CPools::ms_pPedPool) return false;
    for (CPed* current : *CPools::ms_pPedPool) if (current == AsPed(ped)) return true;
    return false;
}
bool IsMission(void* ped) { return ped && AsPed(ped)->m_nPedStatus == 2; }
bool IsCop(const void* ped) { return ped && static_cast<const CPed*>(ped)->m_nPedType == PED_TYPE_COP; }
bool IsGang(const void* ped) {
    if (!ped) return false;
    const int type = static_cast<const CPed*>(ped)->m_nPedType;
    return type >= PED_TYPE_GANG1 && type <= PED_TYPE_GANG9;
}
void ClearAiming(void* ped) { if (ped) AsPed(ped)->ClearLookFlag(); }
void Delete(void* ped) {
    if (!IsValid(ped) || IsPlayer(ped)) return;
    plugin::Command<plugin::Commands::DELETE_CHAR>(CPools::GetPedRef(AsPed(ped)));
}
void ApplyOptions(void* ped, const Types::PedSpawnOptions& options) {
    if (!IsValid(ped)) return;
    const int handle = CPools::GetPedRef(AsPed(ped));
    plugin::Command<plugin::Commands::SET_CHAR_HEALTH>(handle, static_cast<int>(options.health));
    plugin::Command<plugin::Commands::SET_CHAR_ARMOUR>(handle, static_cast<int>(options.armour));
    plugin::Command<plugin::Commands::FREEZE_CHAR_POSITION>(handle, options.freeze);
    if (options.weaponModel) {
        plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(handle, options.weaponModel, 9999);
    }
}
void* SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !IsValidModel(modelId)) return nullptr;
    const int model = static_cast<int>(modelId);
    CVector position = player->TransformFromObjectSpace(CVector(0.0f, 3.0f, 0.0f));
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    int handle = 0;
    plugin::Command<plugin::Commands::CREATE_CHAR>(options.pedType, model, position.x, position.y, position.z, &handle);
    CPed* ped = CPools::GetPed(handle);
    ApplyOptions(ped, options);
    CStreaming::SetModelIsDeletable(model);
    return ped;
}
void* SpawnAtMarker(unsigned int modelId, const Types::PedSpawnOptions& options) {
    const unsigned int index = static_cast<unsigned int>(LOWORD(FrontEndMenuManager.m_nTargetBlipIndex));
    if (index >= MAX_RADAR_TRACES || !IsValidModel(modelId)) return nullptr;
    const auto& trace = CRadar::ms_RadarTrace[index];
    if (trace.m_nRadarSprite != RADAR_SPRITE_WAYPOINT) return nullptr;
    CVector position = trace.m_vecPos;
    const int model = static_cast<int>(modelId);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    int handle = 0;
    plugin::Command<plugin::Commands::CREATE_CHAR>(options.pedType, model, position.x, position.y, position.z, &handle);
    CPed* ped = CPools::GetPed(handle);
    ApplyOptions(ped, options);
    CStreaming::SetModelIsDeletable(model);
    return ped;
}

void SetElvisEverywhere(bool) {}
void SetEveryoneArmed(bool enable) { plugin::patch::Set<bool>(0xA10AB3, enable, false); }
void SetPedsMayhem(bool) {}
void SetPedsAtkRocket(bool) {}
void SetPedsRiot(bool) {}
void SetSlutMagnet(bool enable) { plugin::patch::Set<bool>(0xA10B5F, enable, false); }
void SetGangsControl(bool) {}
void SetGangsEverywhere(bool) {}
void SetNoProstitutes(bool enable) { plugin::patch::Set<bool>(0xA10B99, enable, false); }
void SetNastyLimbs(bool) {}
void SetGangWarsActive(bool) {}
void StartGangWar(bool) {}
void EndGangWar() {}
int GetGangZoneDensity(int) { return 0; }
void SetGangZoneDensity(int, int) {}
unsigned int GetGangMemberModel(unsigned int, unsigned int) { return 0; }
void SetGangMemberModel(unsigned int, unsigned int, unsigned int) {}
void ResetGangModels() {}
void SetGangWeapons(unsigned int, int, int, int) {}

} // namespace XBase::Detail::PedBackend