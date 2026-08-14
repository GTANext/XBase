#include "VehicleBackend.h"

#include "CModelInfo.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CVehicle.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

#include <cmath>

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

bool IsAircraftModel(int model) {
    return CModelInfo::IsHeliModel(model) || CModelInfo::IsPlaneModel(model)
        || model == 155 || model == 165 || model == 177 || model == 180 || model == 181
        || model == 190 || model == 199 || model == 217 || model == 218 || model == 227;
}

bool IsSpawnPositionClear(const CVector& pos) {
    for (CVehicle* vehicle : CPools::ms_pVehiclePool) {
        if (!vehicle) continue;
        const CVector other = vehicle->GetPosition();
        const float dx = other.x - pos.x;
        const float dy = other.y - pos.y;
        const float dz = other.z - pos.z;
        if (dx * dx + dy * dy + dz * dz < 64.0f) return false;
    }
    return !plugin::Command<plugin::Commands::IS_POINT_OBSCURED_BY_A_MISSION_ENTITY>(
        pos.x - 3.0f, pos.y - 3.0f, pos.z - 1.0f,
        pos.x + 3.0f, pos.y + 3.0f, pos.z + 3.0f);
}

CVector FindSideSpawnPosition(CPlayerPed* player, const CVector& origin) {
    if (!player) return origin;

    const float heading = player->GetHeading() * 0.01745329252f;
    const float forwardX = std::sin(heading);
    const float forwardY = std::cos(heading);
    const float rightX = forwardY;
    const float rightY = -forwardX;

    const float offsets[][2] = {
        { 8.0f, 2.0f },
        { -8.0f, 2.0f },
        { 10.0f, -2.0f },
        { -10.0f, -2.0f },
        { 0.0f, 12.0f },
        { 0.0f, -12.0f },
        { 14.0f, 0.0f },
        { -14.0f, 0.0f }
    };

    for (const auto& offset : offsets) {
        CVector candidate(
            origin.x + rightX * offset[0] + forwardX * offset[1],
            origin.y + rightY * offset[0] + forwardY * offset[1],
            origin.z + 3.0f
        );

        float groundZ = candidate.z;
        if (plugin::Command<plugin::Commands::GET_GROUND_Z_FOR_3D_COORD>(candidate.x, candidate.y, candidate.z + 20.0f, &groundZ)) {
            candidate.z = groundZ + 1.0f;
        }

        if (IsSpawnPositionClear(candidate)) {
            return candidate;
        }
    }

    return CVector(origin.x + rightX * 14.0f, origin.y + rightY * 14.0f, origin.z + 1.0f);
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

void MarkNoLongerNeeded(void* vehicle) {
    if (!IsValid(vehicle) || IsPlayerUsing(vehicle)) return;
    const int handle = CPools::GetVehicleRef(AsVehicle(vehicle));
    if (handle == 0) return;
    plugin::Command<plugin::Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(handle);
}

bool Spawn(unsigned int modelId, bool asDriver, bool aircraftInAir, bool cleanupPrevious) {
    CPlayerPed* player = FindPlayerPed();
    const int model = static_cast<int>(modelId);
    if (!player || !IsSupportedModel(model)) return false;
    const int hplayer = CPools::GetPedRef(player);
    const int interior = player->m_nAreaCode;
    CVector position = player->GetPosition();
    float speed = 0.0f;

    CVehicle* previousVehicle = nullptr;
    int previousVehicleHandle = 0;

    if (asDriver && cleanupPrevious && plugin::Command<plugin::Commands::IS_CHAR_IN_ANY_CAR>(hplayer)) {
        previousVehicle = player->m_pVehicle;
        if (previousVehicle) {
            previousVehicleHandle = CPools::GetVehicleRef(previousVehicle);
            position = previousVehicle->GetPosition();
            plugin::Command<plugin::Commands::GET_CAR_SPEED>(previousVehicleHandle, &speed);
            plugin::Command<plugin::Commands::WARP_CHAR_FROM_CAR_TO_COORD>(hplayer, position.x, position.y, position.z);
        }
    } else if (!cleanupPrevious) {
        position = FindSideSpawnPosition(player, position);
    }

    if (interior == 0) {
        if (aircraftInAir && IsAircraftModel(model)) {
            position.z = 400.0f;
        } else if (cleanupPrevious) {
            position.z -= 5.0f;
        }
    }

    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);

    int handle = 0;
    if (asDriver) {
        plugin::Command<plugin::Commands::CREATE_CAR>(model, position.x, position.y, position.z + (cleanupPrevious ? 4.0f : 1.0f), &handle);
    } else {
        if (cleanupPrevious) {
            position = player->TransformFromObjectSpace(CVector(0.0f, 10.0f, 0.0f));
        }
        plugin::Command<plugin::Commands::CREATE_CAR>(model, position.x, position.y, position.z + 1.0f, &handle);
    }
    if (handle == 0) return false;

    CStreaming::SetModelIsDeletable(model);
    CVehicle* vehicle = CPools::GetVehicle(handle);
    if (!vehicle) return false;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    player->GetOrientation(x, y, z);
    vehicle->SetOrientation(x, y, z);
    vehicle->m_eDoorLock = DOORLOCK_UNLOCKED;
    vehicle->m_nAreaCode = interior;

    if (asDriver) {
        plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(hplayer, handle);
        if (speed > 0.0f) ApplySpeed(vehicle, speed);
    }

    if (previousVehicleHandle != 0 && previousVehicleHandle != handle) {
        plugin::Command<plugin::Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(previousVehicleHandle);
    }

    if (!asDriver) {
        plugin::Command<plugin::Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(handle);
    }
    plugin::Command<plugin::Commands::RESTORE_CAMERA_JUMPCUT>();
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