#include "PedBackend.h"

#include "CModelInfo.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CPed.h"
#include "CStreaming.h"
#include "CRadar.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::PedBackend {
namespace { CPed* AsPed(void* ped) { return static_cast<CPed*>(ped); } }

void* GetPlayer() { return FindPlayerPed(); }
bool IsPlayer(void* ped) { return ped && ped == FindPlayerPed(); }
bool IsValidModel(unsigned int modelId) { return CModelInfo::IsPedModel(static_cast<int>(modelId)); }
bool IsValid(void* ped) {
    if (!ped || !CPools::ms_pPedPool) return false;
    for (CPed* current : *CPools::ms_pPedPool) if (current == AsPed(ped)) return true;
    return false;
}
bool IsMission(void* ped) { return ped && AsPed(ped)->m_nCharCreatedBy == 2; }
bool IsCop(const void* ped) { return ped && static_cast<const CPed*>(ped)->m_ePedType == PED_TYPE_COP; }
bool IsGang(const void* ped) {
    if (!ped) return false;
    const int type = static_cast<const CPed*>(ped)->m_ePedType;
    return type >= 7 && type <= 12;
}
void ClearAiming(void* ped) { if (ped) AsPed(ped)->ClearObjective(); }
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
    if (options.weaponModel) plugin::Command<plugin::Commands::GIVE_WEAPON_TO_CHAR>(handle, options.weaponModel, 9999);
}
void* SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !IsValidModel(modelId)) return nullptr;
    const int model = static_cast<int>(modelId);
    const CVector position = player->TransformFromObjectSpace(CVector(0.0f, 3.0f, 0.0f));
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    int handle = 0;
    plugin::Command<plugin::Commands::CREATE_CHAR>(options.pedType, model, position.x, position.y, position.z, &handle);
    CPed* ped = CPools::GetPed(handle);
    ApplyOptions(ped, options);
    CStreaming::SetModelIsDeletable(model);
    return ped;
}
void* SpawnAtMarker(unsigned int, const Types::PedSpawnOptions&) { return nullptr; }
void SetElvisEverywhere(bool) {}
void SetEveryoneArmed(bool) {}
void SetPedsMayhem(bool) {}
void SetPedsAtkRocket(bool) {}
void SetPedsRiot(bool) {}
void SetSlutMagnet(bool) {}
void SetGangsControl(bool) {}
void SetGangsEverywhere(bool) {}
void SetNoProstitutes(bool) {}
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