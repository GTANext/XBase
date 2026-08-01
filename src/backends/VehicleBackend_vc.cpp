#include "VehicleBackend.h"

#include "CModelInfo.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CVehicle.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::VehicleBackend {
void ApplySpeed(void* vehicle, float speed);

namespace {
CVehicle* AsVehicle(void* vehicle) { return static_cast<CVehicle*>(vehicle); }

bool IsSupportedModel(int model) {
    if (model < 130 || model > 236 || !CModelInfo::IsVehicleModel(model)) return false;
    return model != 180 && model != 181 && model != 217;
}

int ClampColor(int color) {
    if (color < 0) return 0;
    return color > 255 ? 255 : color;
}
}

bool IsValidModel(unsigned int modelId) {
    return IsSupportedModel(static_cast<int>(modelId));
}

void* GetCurrent() {
    CPlayerPed* player = FindPlayerPed();
    return player && player->m_bInVehicle ? player->m_pVehicle : nullptr;
}

bool IsValid(void* vehicle) {
    if (!vehicle || !CPools::ms_pVehiclePool) return false;
    for (CVehicle* current : CPools::ms_pVehiclePool) {
        if (current == vehicle) return true;
    }
    return false;
}

int GetId(void* vehicle) {
    return IsValid(vehicle) ? CPools::GetVehicleRef(AsVehicle(vehicle)) : -1;
}

unsigned int GetModelId(void* vehicle) {
    return IsValid(vehicle) ? static_cast<unsigned int>(AsVehicle(vehicle)->m_nModelIndex) : 0u;
}

bool IsPlayerUsing(void* vehicle) {
    CPlayerPed* player = FindPlayerPed();
    return vehicle && player && player->m_pVehicle == AsVehicle(vehicle);
}

bool Delete(void* vehicle) {
    if (!IsValid(vehicle) || IsPlayerUsing(vehicle)) return false;
    const int handle = CPools::GetVehicleRef(AsVehicle(vehicle));
    if (handle == 0) return false;
    plugin::Command<plugin::Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(handle);
    plugin::Command<plugin::Commands::DELETE_CAR>(handle);
    return true;
}

void Repair(void* vehicle) { if (vehicle) AsVehicle(vehicle)->m_fHealth = 1000.0f; }
void Start(void* vehicle) { ApplySpeed(vehicle, 40.0f); }
void Stop(void* vehicle) {
    if (!vehicle) return;
    AsVehicle(vehicle)->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
    AsVehicle(vehicle)->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
}
void SetEngine(void* vehicle, bool enable) { if (vehicle) AsVehicle(vehicle)->bEngineOn = enable; }
void Unflip(void* vehicle) {
    if (!vehicle) return;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    AsVehicle(vehicle)->GetOrientation(x, y, z);
    AsVehicle(vehicle)->SetOrientation(x, y + 3.14159265f, z);
    Stop(vehicle);
}
void SetHeavy(void* vehicle, bool enable) {
    if (vehicle) plugin::Command<plugin::Commands::SET_CAR_HEAVY>(CPools::GetVehicleRef(AsVehicle(vehicle)), enable);
}
void SetWatertight(void* vehicle, bool enable) {
    if (vehicle) plugin::Command<plugin::Commands::SET_CAR_WATERTIGHT>(CPools::GetVehicleRef(AsVehicle(vehicle)), enable);
}
float GetHealth(void* vehicle) { return vehicle ? AsVehicle(vehicle)->m_fHealth : 0.0f; }
void SetHealth(void* vehicle, float health) { if (vehicle) AsVehicle(vehicle)->m_fHealth = health; }
bool GetLights(void* vehicle) { return vehicle && AsVehicle(vehicle)->bLightsOn; }
void SetLights(void* vehicle, bool enable) { if (vehicle) AsVehicle(vehicle)->bLightsOn = enable; }
bool GetLocked(void* vehicle) { return vehicle && AsVehicle(vehicle)->m_eDoorLock == DOORLOCK_LOCKED_PLAYER_INSIDE; }
void SetLocked(void* vehicle, bool enable) {
    if (vehicle) AsVehicle(vehicle)->m_eDoorLock = enable ? DOORLOCK_LOCKED_PLAYER_INSIDE : DOORLOCK_UNLOCKED;
}
Types::ProofState GetProofState(void* vehicle) {
    Types::ProofState state;
    if (!vehicle) return state;
    CVehicle* value = AsVehicle(vehicle);
    state.bullet = value->bBulletProof;
    state.collision = value->bCollisionProof;
    state.explosion = value->bExplosionProof;
    state.fire = value->bFireProof;
    state.melee = value->bMeleeProof;
    state.nonPlayer = value->bImmuneToNonPlayerDamage;
    return state;
}
void SetProofState(void* vehicle, const Types::ProofState& state) {
    if (!vehicle) return;
    CVehicle* value = AsVehicle(vehicle);
    value->bBulletProof = state.bullet;
    value->bCollisionProof = state.collision;
    value->bExplosionProof = state.explosion;
    value->bFireProof = state.fire;
    value->bMeleeProof = state.melee;
    value->bImmuneToNonPlayerDamage = state.nonPlayer;
}
bool GetVisible(void* vehicle) { return vehicle && AsVehicle(vehicle)->bIsVisible; }
void SetVisible(void* vehicle, bool enable) { if (vehicle) AsVehicle(vehicle)->bIsVisible = enable; }
bool IsUpsideDown(void* vehicle) { return vehicle && AsVehicle(vehicle)->IsUpsideDown(); }

int GetPrimaryColor(void* vehicle) { return vehicle ? AsVehicle(vehicle)->m_nPrimaryColor : -1; }
int GetSecondaryColor(void* vehicle) { return vehicle ? AsVehicle(vehicle)->m_nSecondaryColor : -1; }
void SetColors(void* vehicle, int primary, int secondary, int, int) {
    if (!vehicle) return;
    plugin::Command<plugin::Commands::CHANGE_CAR_COLOUR>(CPools::GetVehicleRef(AsVehicle(vehicle)), ClampColor(primary), ClampColor(secondary));
}
void WarpToSeat(void* vehicle, int seatIndex) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !vehicle) return;
    const int ped = CPools::GetPedRef(player);
    const int car = CPools::GetVehicleRef(AsVehicle(vehicle));
    if (seatIndex <= 0) plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(ped, car);
    else plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR_AS_PASSENGER>(ped, car, seatIndex - 1);
}
void OpenDoor(void* vehicle, int doorIndex) {
    if (!vehicle) return;
    if (doorIndex < 0) doorIndex = 0;
    if (doorIndex > 5) doorIndex = 5;
    AsVehicle(vehicle)->OpenDoor(0, static_cast<eDoors>(doorIndex), 1.0f);
}
#ifdef XBASE_BACKEND_SA
void PopDoor(void* vehicle, int doorIndex) {
    if (!vehicle) return;
    if (doorIndex < 0) doorIndex = 0;
    if (doorIndex > 5) doorIndex = 5;
    AsVehicle(vehicle)->PopDoor(0, static_cast<eDoors>(doorIndex), true);
}
#endif
void BlowUpAll() {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !CPools::ms_pVehiclePool) return;
    for (CVehicle* vehicle : CPools::ms_pVehiclePool) if (vehicle) vehicle->BlowUpCar(player);
}
bool Spawn(unsigned int modelId, bool asDriver, bool aircraftInAir) {
    CPlayerPed* player = FindPlayerPed();
    const int model = static_cast<int>(modelId);
    if (!player || !IsSupportedModel(model)) return false;
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    CVector position = player->TransformFromObjectSpace(CVector(0.0f, 5.0f, 1.0f));
    if (aircraftInAir && (CModelInfo::IsPlaneModel(model) || CModelInfo::IsHeliModel(model))) {
        position.z = 200.0f;
    }
    int handle = 0;
    plugin::Command<plugin::Commands::CREATE_CAR>(model, position.x, position.y, position.z, &handle);
    CStreaming::SetModelIsDeletable(model);
    CVehicle* vehicle = CPools::GetVehicle(handle);
    if (!vehicle) return false;
    if (asDriver) {
        plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(CPools::GetPedRef(player), handle);
    }
    return true;
}
void ApplySpeed(void* vehicle, float speed) {
    if (!vehicle) return;
    const CVector forward = AsVehicle(vehicle)->GetForward();
    const float velocity = speed / 50.0f;
    AsVehicle(vehicle)->m_vecMoveSpeed = CVector(forward.x * velocity, forward.y * velocity, forward.z * velocity);
}

} // namespace XBase::Detail::VehicleBackend