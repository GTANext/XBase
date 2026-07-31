#include <XBase/Vehicle.h>
#include <XBase/Core.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CRadar.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"
#include <deque>
#include <windows.h>

namespace {

bool IsVehicleInPool(CVehicle* vehicle) {
    if (!vehicle) return false;
    for (CVehicle* v : CPools::ms_pVehiclePool) {
        if (v == vehicle) return true;
    }
    return false;
}

bool IsPlayerUsingVehicle(CVehicle* vehicle) {
    if (!vehicle || !IsVehicleInPool(vehicle)) return false;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    return player->m_pVehicle == vehicle;
}

bool DeleteVehicle(CVehicle* vehicle) {
    if (!IsVehicleInPool(vehicle) || IsPlayerUsingVehicle(vehicle)) return false;
    const int handle = CPools::GetVehicleRef(vehicle);
    if (handle == 0) return false;
    plugin::Command<plugin::Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(handle);
    plugin::Command<plugin::Commands::DELETE_CAR>(handle);
    return true;
}

bool IsValidVehicleModel(unsigned int modelId) {
    const int model = static_cast<int>(modelId);
    return CModelInfo::IsVehicleModel(model) && !CModelInfo::IsTrailerModel(model);
}

} // namespace

namespace XBase::Vehicle {

void SetProofState(const Types::ProofState& state);
void Unflip();
void SetHeavy(bool enable);
void SetWatertight(bool enable);

namespace {
    bool s_disableParticles = false;
    float s_speedLock = 0.0f;
    float s_targetSpeed = 0.0f;
    RuntimeOptions s_runtimeOptions;
    SpawnPolicy s_spawnPolicy;
    CVehicle* s_trackedVehicle = nullptr;
    bool s_autoDriveEnabled = false;
    CVector s_autoDriveTarget;
    bool s_hasAutoDriveTarget = false;
    DWORD s_spawnWindowStart = 0;
    unsigned int s_spawnWindowCount = 0;
    bool s_spawnInProgress = false;
    std::deque<VehicleEvent> s_events;

    void PushEvent(VehicleEventType type, SpawnFailureReason reason, unsigned int modelId) {
        if (s_events.size() >= 32) s_events.pop_front();
        s_events.push_back({type, reason, modelId});
    }

    void ResetSpawnSession() {
        s_trackedVehicle = nullptr;
        s_spawnWindowStart = 0;
        s_spawnWindowCount = 0;
        s_spawnInProgress = false;
        s_events.clear();
    }

    Types::ProofState s_savedProofs;
    CVehicle* s_savedVehicle = nullptr;
    bool s_hasSavedProofs = false;

    void RestoreProofs() {
        if (s_hasSavedProofs && s_savedVehicle && IsVehicleInPool(s_savedVehicle)) {
            s_savedVehicle->bBulletProof = s_savedProofs.bullet;
            s_savedVehicle->bCollisionProof = s_savedProofs.collision;
            s_savedVehicle->bExplosionProof = s_savedProofs.explosion;
            s_savedVehicle->bFireProof = s_savedProofs.fire;
            s_savedVehicle->bMeleeProof = s_savedProofs.melee;
        }
        s_savedVehicle = nullptr;
        s_hasSavedProofs = false;
    }

    void ProcessAutoDrive(CVehicle* vehicle) {
        static CVehicle* s_lastVehicle = nullptr;
        static CVector s_lastTarget;
        static bool s_taskIssued = false;

        if (!vehicle || !s_autoDriveEnabled || !s_hasAutoDriveTarget) {
            s_lastVehicle = nullptr;
            s_taskIssued = false;
            return;
        }

        const bool targetChanged = s_lastTarget.x != s_autoDriveTarget.x ||
            s_lastTarget.y != s_autoDriveTarget.y ||
            s_lastTarget.z != s_autoDriveTarget.z;
        if (s_taskIssued && s_lastVehicle == vehicle && !targetChanged) {
            return;
        }

        const int handle = CPools::GetVehicleRef(vehicle);
        if (handle <= 0) {
            s_taskIssued = false;
            return;
        }

        const int model = vehicle->m_nModelIndex;
        if (CModelInfo::IsBoatModel(model)) {
            plugin::Command<plugin::Commands::BOAT_GOTO_COORDS>(handle, s_autoDriveTarget.x, s_autoDriveTarget.y, s_autoDriveTarget.z);
        } else if (CModelInfo::IsPlaneModel(model)) {
            CVector position = vehicle->GetPosition();
            if (position.z < 250.0f) {
                position.z = 300.0f;
                vehicle->SetPosn(position);
            }
            plugin::Command<plugin::Commands::PLANE_GOTO_COORDS>(handle, s_autoDriveTarget.x, s_autoDriveTarget.y, 300.0f, 30, 200);
        } else if (CModelInfo::IsHeliModel(model)) {
            CVector position = vehicle->GetPosition();
            if (position.z < 150.0f) {
                position.z = 200.0f;
                vehicle->SetPosn(position);
            }
            plugin::Command<plugin::Commands::HELI_GOTO_COORDS>(handle, s_autoDriveTarget.x, s_autoDriveTarget.y, 200.0f, 30, 200);
        } else if (!CModelInfo::IsTrainModel(model)) {
            plugin::Command<plugin::Commands::CAR_GOTO_COORDINATES>(handle, s_autoDriveTarget.x, s_autoDriveTarget.y, s_autoDriveTarget.z);
        } else {
            s_taskIssued = false;
            return;
        }

        s_lastVehicle = vehicle;
        s_lastTarget = s_autoDriveTarget;
        s_taskIssued = true;
    }

    void ProcessRuntimeOptions(CVehicle* vehicle) {
        if (!vehicle) {
            RestoreProofs();
            return;
        }

        if (s_runtimeOptions.noDamage) {
            if (!s_hasSavedProofs || s_savedVehicle != vehicle) {
                RestoreProofs();
                s_savedProofs = GetProofState();
                s_savedVehicle = vehicle;
                s_hasSavedProofs = true;
            }
            Types::ProofState proofs;
            proofs.bullet = true;
            proofs.collision = true;
            proofs.explosion = true;
            proofs.fire = true;
            proofs.melee = true;
            SetProofState(proofs);
        } else {
            RestoreProofs();
        }

        if (s_runtimeOptions.autoUnflip && vehicle->IsUpsideDown()) {
            Unflip();
        }
        SetHeavy(s_runtimeOptions.heavy);
        SetWatertight(s_runtimeOptions.watertight);
        if (s_runtimeOptions.speedLock) {
            float speed = vehicle->m_vecMoveSpeed.Magnitude();
            if (speed > s_runtimeOptions.speed) {
                vehicle->m_vecMoveSpeed *= s_runtimeOptions.speed / speed;
            }
        }
    }
}

CVehicle* GetCurrent() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return nullptr;
    const int hplayer = CPools::GetPedRef(player);
    if (!plugin::Command<plugin::Commands::IS_CHAR_IN_ANY_CAR>(hplayer)) return nullptr;
    CVehicle* vehicle = player->m_pVehicle;
    return IsVehicleInPool(vehicle) ? vehicle : nullptr;
}

void SetRuntimeOptions(const RuntimeOptions& options) {
    s_runtimeOptions = options;
    s_speedLock = options.speedLock ? options.speed : 0.0f;
}

void SetSpawnPolicy(const SpawnPolicy& policy) {
    s_spawnPolicy = policy;
}

SpawnPolicy GetSpawnPolicy() {
    return s_spawnPolicy;
}

SpawnResult SpawnEx(unsigned int modelId, const SpawnOptions& options) {
    SpawnResult result;
    if (!IsValidVehicleModel(modelId)) {
        result.failure = SpawnFailureReason::InvalidModel;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    if (s_spawnInProgress) {
        result.failure = SpawnFailureReason::SpawnInProgress;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    const DWORD now = GetTickCount();
    if (s_spawnPolicy.windowMs == 0 || s_spawnPolicy.maxSpawns == 0 ||
        (s_spawnWindowStart != 0 && now - s_spawnWindowStart < s_spawnPolicy.windowMs &&
         s_spawnWindowCount >= s_spawnPolicy.maxSpawns)) {
        result.failure = SpawnFailureReason::RateLimited;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    if (s_spawnWindowStart == 0 || now - s_spawnWindowStart >= s_spawnPolicy.windowMs) {
        s_spawnWindowStart = now;
        s_spawnWindowCount = 0;
    }
    ++s_spawnWindowCount;
    s_spawnInProgress = true;
    const bool spawned = Spawn(modelId, options);
    s_spawnInProgress = false;
    if (!spawned) {
        result.failure = SpawnFailureReason::BackendRejected;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    result.success = true;
    result.vehicle = GetCurrent();
    if (options.asDriver && result.vehicle) {
        if (options.cleanupPrevious && s_trackedVehicle && s_trackedVehicle != result.vehicle &&
            IsVehicleInPool(s_trackedVehicle)) {
            if (IsPlayerUsingVehicle(s_trackedVehicle) ||
                !DeleteVehicle(s_trackedVehicle)) {
                PushEvent(VehicleEventType::PreviousVehicleCleanupSkipped, SpawnFailureReason::None, modelId);
            } else {
                PushEvent(VehicleEventType::PreviousVehicleCleaned, SpawnFailureReason::None, modelId);
            }
        }
        s_trackedVehicle = result.vehicle;
    }
    PushEvent(VehicleEventType::Spawned, SpawnFailureReason::None, modelId);
    return result;
}

bool PollEvent(VehicleEvent& event) {
    if (s_events.empty()) return false;
    event = s_events.front();
    s_events.pop_front();
    return true;
}

void NotifyGameInit() {
    RestoreProofs();
    ResetSpawnSession();
}

void Shutdown() {
    RestoreProofs();
    ResetSpawnSession();
    s_autoDriveEnabled = false;
    s_hasAutoDriveTarget = false;
    s_autoDriveTarget = CVector(0.0f, 0.0f, 0.0f);
    s_spawnPolicy = SpawnPolicy{};
    s_disableParticles = false;
    s_runtimeOptions = RuntimeOptions{};
    s_speedLock = 0.0f;
    s_targetSpeed = 0.0f;
}

void Process() {
    if (!Core::IsWorldReady()) return;
    CVehicle* vehicle = GetCurrent();
    ProcessRuntimeOptions(vehicle);
    ProcessAutoDrive(vehicle);

    if (vehicle) {
        if (s_disableParticles) {
            vehicle->bDisableParticles = true;
        }

        if (s_speedLock > 0.0f) {
            float speed = vehicle->m_vecMoveSpeed.Magnitude();
            if (speed > s_speedLock) {
                vehicle->m_vecMoveSpeed *= s_speedLock / speed;
            }
        }

        if (s_targetSpeed > 0.0f) {
            CVector forward = vehicle->GetForward();
            CVector targetVel = forward * s_targetSpeed;
            vehicle->m_vecMoveSpeed = targetVel;
        }
    }
}

void Repair() {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->Fix();
    vehicle->m_fHealth = 1000.0f;
}

void Start() {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    CPlayerPed* player = FindPlayerPed();
    if (!player || player->m_pVehicle != vehicle) return;
    const CVector forward = vehicle->GetForward();
    vehicle->m_vecMoveSpeed = CVector(forward.x * 0.8f, forward.y * 0.8f, forward.z * 0.8f);
}

void Stop() {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
    vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
}

void SetEngine(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->bEngineBroken = !enable;
    vehicle->bEngineOn = enable;
}

void Unflip() {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle || !vehicle->IsUpsideDown()) return;
    const int handle = CPools::GetVehicleRef(vehicle);
    float roll = 0.0f;
    plugin::Command<plugin::Commands::GET_CAR_ROLL>(handle, &roll);
    roll += 180.0f;
    plugin::Command<plugin::Commands::SET_CAR_ROLL>(handle, roll);
    plugin::Command<plugin::Commands::SET_CAR_ROLL>(handle, roll);
    Stop();
}

void SetHeavy(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    plugin::Command<plugin::Commands::SET_CAR_HEAVY>(CPools::GetVehicleRef(vehicle), enable);
}

void SetWatertight(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    plugin::Command<plugin::Commands::SET_CAR_WATERTIGHT>(CPools::GetVehicleRef(vehicle), enable);
}

float GetHealth() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->m_fHealth : 0.0f;
}

void SetHealth(float health) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->m_fHealth = health;
}

bool GetLights() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bLightsOn : false;
}

void SetLights(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->bLightsOn = enable;
    vehicle->m_nOverrideLights = enable ? 2 : 1;
}

bool GetLocked() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->m_eDoorLock == DOORLOCK_LOCKED_PLAYER_INSIDE : false;
}

void SetLocked(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->m_eDoorLock = enable ? DOORLOCK_LOCKED_PLAYER_INSIDE : DOORLOCK_UNLOCKED;
}

Types::ProofState GetProofState() {
    Types::ProofState state;
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return state;
    state.bullet = vehicle->bBulletProof;
    state.collision = vehicle->bCollisionProof;
    state.explosion = vehicle->bExplosionProof;
    state.fire = vehicle->bFireProof;
    state.melee = vehicle->bMeleeProof;
    return state;
}

void SetProofState(const Types::ProofState& state) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->bBulletProof = state.bullet;
    vehicle->bCollisionProof = state.collision;
    vehicle->bExplosionProof = state.explosion;
    vehicle->bFireProof = state.fire;
    vehicle->bMeleeProof = state.melee;
}

bool GetVisible() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bIsVisible : false;
}

void SetVisible(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bIsVisible = enable;
}

bool GetAlwaysSkidMarks() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bAlwaysSkidMarks : false;
}

void SetAlwaysSkidMarks(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bAlwaysSkidMarks = enable;
}

bool GetDriverTargetable() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bVehicleCanBeTargetted : false;
}

void SetDriverTargetable(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bVehicleCanBeTargetted = enable;
}

bool GetHeatSeekingTargetable() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bVehicleCanBeTargettedByHS : false;
}

void SetHeatSeekingTargetable(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bVehicleCanBeTargettedByHS = enable;
}

bool GetPetrolTankWeakPoint() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bPetrolTankIsWeakPoint : false;
}

void SetPetrolTankWeakPoint(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bPetrolTankIsWeakPoint = enable;
}

bool GetSirenOrAlarm() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bSirenOrAlarm : false;
}

void SetSirenOrAlarm(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bSirenOrAlarm = enable;
}

bool GetTakeLessDamage() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->bTakeLessDamage : false;
}

void SetTakeLessDamage(bool enable) {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->bTakeLessDamage = enable;
}

Colors GetColors() {
    Colors colors;
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return colors;
    colors.primary = vehicle->m_nPrimaryColor;
    colors.secondary = vehicle->m_nSecondaryColor;
    colors.tertiary = vehicle->m_nTertiaryColor;
    colors.quaternary = vehicle->m_nQuaternaryColor;
    return colors;
}

void SetColors(const Colors& colors) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->m_nPrimaryColor = static_cast<unsigned char>(colors.primary);
    vehicle->m_nSecondaryColor = static_cast<unsigned char>(colors.secondary);
    vehicle->m_nTertiaryColor = static_cast<unsigned char>(colors.tertiary);
    vehicle->m_nQuaternaryColor = static_cast<unsigned char>(colors.quaternary);
    plugin::Command<plugin::Commands::CHANGE_CAR_COLOUR>(
        CPools::GetVehicleRef(vehicle), vehicle->m_nPrimaryColor, vehicle->m_nSecondaryColor);
}

int GetPrimaryColor() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->m_nPrimaryColor : 0;
}

int GetSecondaryColor() {
    CVehicle* vehicle = GetCurrent();
    return vehicle ? vehicle->m_nSecondaryColor : 0;
}

void SetPrimaryColor(int color) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->m_nPrimaryColor = static_cast<unsigned char>(color);
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::CHANGE_CAR_COLOUR>(hveh, vehicle->m_nPrimaryColor, vehicle->m_nSecondaryColor);
}

void SetSecondaryColor(int color) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->m_nSecondaryColor = static_cast<unsigned char>(color);
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::CHANGE_CAR_COLOUR>(hveh, vehicle->m_nPrimaryColor, vehicle->m_nSecondaryColor);
}

int GetPaintjob() {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return -1;
    int result = -1;
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::GET_CURRENT_VEHICLE_PAINTJOB>(hveh, &result);
    return result;
}

bool SetPaintjob(int paintjob) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return false;
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::GIVE_VEHICLE_PAINTJOB>(hveh, paintjob);
    return true;
}

void AddUpgrade(unsigned int modelId) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    const int model = static_cast<int>(modelId);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    vehicle->AddVehicleUpgrade(model);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
}

void RemoveUpgrade(unsigned int modelId) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    vehicle->RemoveVehicleUpgrade(static_cast<int>(modelId));
}

void RemoveAllUpgrades() {
    CVehicle* vehicle = GetCurrent();
    if (vehicle) vehicle->RemoveAllUpgrades();
}

int GetUpgrade(int slot) {
    CVehicle* vehicle = GetCurrent();
    if (!vehicle || slot < 0 || slot >= 15) return -1;
    return vehicle->m_anUpgrades[slot];
}

void OpenDoor(int doorIndex) {
    if (!Core::IsWorldReady()) return;
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::OPEN_CAR_DOOR>(hveh, doorIndex);
}

void PopDoor(int doorIndex) {
    if (!Core::IsWorldReady()) return;
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    int hveh = CPools::GetVehicleRef(vehicle);
    plugin::Command<plugin::Commands::POP_CAR_DOOR>(hveh, doorIndex, false);
}

void WarpToSeat(int seatIndex) {
    if (!Core::IsWorldReady()) return;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    CVehicle* vehicle = GetCurrent();
    if (!vehicle) return;
    int hveh = CPools::GetVehicleRef(vehicle);
    int hplayer = CPools::GetPedRef(player);
    if (seatIndex == 0) {
        plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(hplayer, hveh);
    } else {
        plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR_AS_PASSENGER>(hplayer, hveh, seatIndex);
    }
}

void SetAutoDriveToWaypoint(bool enable) {
    if (!enable) {
        s_autoDriveEnabled = false;
        s_hasAutoDriveTarget = false;
        return;
    }
    const auto index = static_cast<unsigned int>(LOWORD(FrontEndMenuManager.m_nTargetBlipIndex));
    if (index >= MAX_RADAR_TRACES || CRadar::ms_RadarTrace[index].m_nRadarSprite != RADAR_SPRITE_WAYPOINT) {
        s_autoDriveEnabled = false;
        s_hasAutoDriveTarget = false;
        return;
    }
    const tRadarTrace& blip = CRadar::ms_RadarTrace[index];
    s_autoDriveTarget = blip.m_vecPos;
    s_autoDriveEnabled = true;
    s_hasAutoDriveTarget = true;
}

void SetTrafficDensity(float density) {
    *reinterpret_cast<float*>(0x8A5B20) = density;
}

bool GetDisableParticles() {
    return s_disableParticles;
}

void SetDisableParticles(bool enable) {
    s_disableParticles = enable;
}

void ApplySpeedLock(float speed) {
    s_speedLock = speed;
}

void ApplyTargetSpeed(float speed) {
    s_targetSpeed = speed;
}

void RestoreTargetSpeed() {
    s_targetSpeed = 0.0f;
}

void BlowUpAll() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    for (CVehicle* vehicle : CPools::ms_pVehiclePool) {
        if (vehicle) vehicle->BlowUpCar(player, false);
    }
}

bool Spawn(unsigned int modelId, const SpawnOptions& options) {
    if (!IsValidVehicleModel(modelId)) return false;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;

    const int model = static_cast<int>(modelId);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);

    CVector pos = player->GetPosition();
    pos.x += 5.0f;
    if (options.aircraftInAir && (CModelInfo::IsPlaneModel(model) || CModelInfo::IsHeliModel(model))) {
        pos.z = 300.0f;
    }

    int hveh = 0;
    plugin::Command<plugin::Commands::CREATE_CAR>(model, pos.x, pos.y, pos.z, &hveh);
    if (hveh == 0) return false;
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);

    CVehicle* spawned = reinterpret_cast<CVehicle*>(CPools::GetVehicle(hveh));
    if (!spawned) return false;
    if (options.asDriver) {
        plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(CPools::GetPedRef(player), hveh);
    }
    return true;
}

bool Spawn(unsigned int modelId) {
    return Spawn(modelId, SpawnOptions{});
}

} // namespace XBase::Vehicle
