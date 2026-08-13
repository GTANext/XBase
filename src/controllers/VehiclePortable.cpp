#include <XBase/Vehicle.h>

#include "../backends/VehicleBackend.h"

#include <cstdint>
#include <deque>
#include <windows.h>

namespace XBase::Vehicle {
namespace {
float s_speedLock = 0.0f;
float s_targetSpeed = 0.0f;
RuntimeOptions s_runtimeOptions;
SpawnPolicy s_spawnPolicy;
void* s_trackedVehicle = nullptr;
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
void* s_savedVehicle = nullptr;
bool s_hasSavedProofs = false;

void RestoreProofs() {
    if (s_hasSavedProofs && Detail::VehicleBackend::IsValid(s_savedVehicle)) {
        Detail::VehicleBackend::SetProofState(s_savedVehicle, s_savedProofs);
    }
    s_savedVehicle = nullptr;
    s_hasSavedProofs = false;
}

void ProcessRuntimeOptions(void* vehicle) {
    if (!vehicle) {
        RestoreProofs();
        return;
    }

    if (s_runtimeOptions.noDamage) {
        if (!s_hasSavedProofs || s_savedVehicle != vehicle) {
            RestoreProofs();
            s_savedProofs = Detail::VehicleBackend::GetProofState(vehicle);
            s_savedVehicle = vehicle;
            s_hasSavedProofs = true;
        }
        Types::ProofState proofs;
        proofs.bullet = true;
        proofs.collision = true;
        proofs.explosion = true;
        proofs.fire = true;
        proofs.melee = true;
        proofs.nonPlayer = true;
        Detail::VehicleBackend::SetProofState(vehicle, proofs);
    } else {
        RestoreProofs();
    }

    if (s_runtimeOptions.autoUnflip && Detail::VehicleBackend::IsUpsideDown(vehicle)) {
        Detail::VehicleBackend::Unflip(vehicle);
    }
    Detail::VehicleBackend::SetHeavy(vehicle, s_runtimeOptions.heavy);
    Detail::VehicleBackend::SetWatertight(vehicle, s_runtimeOptions.watertight);
    if (s_runtimeOptions.speedLock) {
        Detail::VehicleBackend::ApplySpeed(vehicle, s_runtimeOptions.speed);
    }
}
}

static void* GetCurrentObject() {
    return Detail::VehicleBackend::GetCurrent();
}

VehicleId GetCurrentId() {
    const int ref = Detail::VehicleBackend::GetId(GetCurrentObject());
    return VehicleId{ref >= 0 ? static_cast<std::uint32_t>(ref) + 1u : 0u};
}

VehicleSnapshot GetSnapshot() {
    VehicleSnapshot snapshot;
    void* vehicle = GetCurrentObject();
    if (!vehicle || !Detail::VehicleBackend::IsValid(vehicle)) return snapshot;

    snapshot.id = GetCurrentId();
    snapshot.modelId = Detail::VehicleBackend::GetModelId(vehicle);
    snapshot.health = Detail::VehicleBackend::GetHealth(vehicle);
    snapshot.colors.primary = Detail::VehicleBackend::GetPrimaryColor(vehicle);
    snapshot.colors.secondary = Detail::VehicleBackend::GetSecondaryColor(vehicle);
    snapshot.lights = Detail::VehicleBackend::GetLights(vehicle);
    snapshot.locked = Detail::VehicleBackend::GetLocked(vehicle);
    snapshot.visible = Detail::VehicleBackend::GetVisible(vehicle);
    snapshot.proofs = Detail::VehicleBackend::GetProofState(vehicle);
    return snapshot;
}

void SetRuntimeOptions(const RuntimeOptions& options) {
    s_runtimeOptions = options;
    s_speedLock = options.speedLock ? options.speed : 0.0f;
}

bool SetAutoDriveToWaypoint(bool) { return false; }

bool SetTrafficDensity(float) {
    return false;
}

void SetSpawnPolicy(const SpawnPolicy& policy) {
    s_spawnPolicy = policy;
}

SpawnPolicy GetSpawnPolicy() {
    return s_spawnPolicy;
}

SpawnResult SpawnEx(unsigned int modelId, const SpawnOptions& options) {
    SpawnResult result;
    if (s_spawnInProgress) {
        result.failure = SpawnFailureReason::SpawnInProgress;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    if (!Detail::VehicleBackend::IsValidModel(modelId)) {
        result.failure = SpawnFailureReason::InvalidModel;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }

    if (s_spawnPolicy.windowMs == 0 || s_spawnPolicy.maxSpawns == 0) {
        result.failure = SpawnFailureReason::RateLimited;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    const DWORD now = GetTickCount();
    if (s_spawnWindowStart == 0 || now - s_spawnWindowStart >= s_spawnPolicy.windowMs) {
        s_spawnWindowStart = now;
        s_spawnWindowCount = 0;
    }
    if (s_spawnWindowCount >= s_spawnPolicy.maxSpawns) {
        result.failure = SpawnFailureReason::RateLimited;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    ++s_spawnWindowCount;
    s_spawnInProgress = true;
    const bool ok = Detail::VehicleBackend::Spawn(modelId, options.asDriver, options.aircraftInAir);
    s_spawnInProgress = false;
    if (!ok) {
        result.failure = SpawnFailureReason::BackendRejected;
        PushEvent(VehicleEventType::SpawnRejected, result.failure, modelId);
        return result;
    }
    result.success = true;
    void* current = GetCurrentObject();
    const int ref = Detail::VehicleBackend::GetId(current);
    result.vehicle = VehicleId{ref >= 0 ? static_cast<std::uint32_t>(ref) + 1u : 0u};
    if (options.asDriver && current) {
        if (options.cleanupPrevious && s_trackedVehicle && s_trackedVehicle != current &&
            Detail::VehicleBackend::IsValid(s_trackedVehicle)) {
            if (Detail::VehicleBackend::IsPlayerUsing(s_trackedVehicle) ||
                !Detail::VehicleBackend::Delete(s_trackedVehicle)) {
                PushEvent(VehicleEventType::PreviousVehicleCleanupSkipped, SpawnFailureReason::None, modelId);
            } else {
                PushEvent(VehicleEventType::PreviousVehicleCleaned, SpawnFailureReason::None, modelId);
            }
        }
        s_trackedVehicle = current;
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

void Process() {
    if (s_trackedVehicle && !Detail::VehicleBackend::IsValid(s_trackedVehicle)) {
        s_trackedVehicle = nullptr;
    }
    void* vehicle = Detail::VehicleBackend::GetCurrent();
    ProcessRuntimeOptions(vehicle);
    if (!vehicle) return;
    if (s_targetSpeed > 0.0f) {
        Detail::VehicleBackend::ApplySpeed(vehicle, s_targetSpeed);
    } else if (s_speedLock > 0.0f && !s_runtimeOptions.speedLock) {
        Detail::VehicleBackend::ApplySpeed(vehicle, s_speedLock);
    }
}

void Shutdown() {
    RestoreProofs();
    ResetSpawnSession();
    s_runtimeOptions = RuntimeOptions{};
    s_spawnPolicy = SpawnPolicy{};
    s_speedLock = 0.0f;
    s_targetSpeed = 0.0f;
}

void Repair() { Detail::VehicleBackend::Repair(Detail::VehicleBackend::GetCurrent()); }
void Start() { Detail::VehicleBackend::Start(Detail::VehicleBackend::GetCurrent()); }
void Stop() { Detail::VehicleBackend::Stop(Detail::VehicleBackend::GetCurrent()); }
void SetEngine(bool enable) { Detail::VehicleBackend::SetEngine(Detail::VehicleBackend::GetCurrent(), enable); }
void Unflip() { Detail::VehicleBackend::Unflip(Detail::VehicleBackend::GetCurrent()); }
void SetHeavy(bool enable) { Detail::VehicleBackend::SetHeavy(Detail::VehicleBackend::GetCurrent(), enable); }
void SetWatertight(bool enable) { Detail::VehicleBackend::SetWatertight(Detail::VehicleBackend::GetCurrent(), enable); }
float GetHealth() { return Detail::VehicleBackend::GetHealth(Detail::VehicleBackend::GetCurrent()); }
void SetHealth(float health) { Detail::VehicleBackend::SetHealth(Detail::VehicleBackend::GetCurrent(), health); }
bool GetLights() { return Detail::VehicleBackend::GetLights(Detail::VehicleBackend::GetCurrent()); }
void SetLights(bool enable) { Detail::VehicleBackend::SetLights(Detail::VehicleBackend::GetCurrent(), enable); }
bool GetLocked() { return Detail::VehicleBackend::GetLocked(Detail::VehicleBackend::GetCurrent()); }
void SetLocked(bool enable) { Detail::VehicleBackend::SetLocked(Detail::VehicleBackend::GetCurrent(), enable); }
Types::ProofState GetProofState() { return Detail::VehicleBackend::GetProofState(Detail::VehicleBackend::GetCurrent()); }
void SetProofState(const Types::ProofState& state) { Detail::VehicleBackend::SetProofState(Detail::VehicleBackend::GetCurrent(), state); }
bool GetVisible() { return Detail::VehicleBackend::GetVisible(Detail::VehicleBackend::GetCurrent()); }
void SetVisible(bool enable) { Detail::VehicleBackend::SetVisible(Detail::VehicleBackend::GetCurrent(), enable); }
bool TryGetAlwaysSkidMarks(bool&) { return false; }
bool SetAlwaysSkidMarks(bool) { return false; }
bool TryGetDisableParticles(bool&) { return false; }
bool SetDisableParticles(bool) { return false; }
bool TryGetDriverTargetable(bool&) { return false; }
bool SetDriverTargetable(bool) { return false; }
bool TryGetHeatSeekingTargetable(bool&) { return false; }
bool SetHeatSeekingTargetable(bool) { return false; }
bool TryGetPetrolTankWeakPoint(bool&) { return false; }
bool SetPetrolTankWeakPoint(bool) { return false; }
bool TryGetSirenOrAlarm(bool&) { return false; }
bool SetSirenOrAlarm(bool) { return false; }
bool TryGetTakeLessDamage(bool&) { return false; }
bool SetTakeLessDamage(bool) { return false; }
int GetPrimaryColor();
int GetSecondaryColor();

void SetColors(const Colors& colors) {
    void* vehicle = Detail::VehicleBackend::GetCurrent();
    Detail::VehicleBackend::SetColors(vehicle, colors.primary, colors.secondary, colors.tertiary, colors.quaternary);
}
Colors GetColors() {
    Colors colors;
    colors.primary = GetPrimaryColor();
    colors.secondary = GetSecondaryColor();
    return colors;
}

int GetPrimaryColor() { return Detail::VehicleBackend::GetPrimaryColor(Detail::VehicleBackend::GetCurrent()); }
int GetSecondaryColor() { return Detail::VehicleBackend::GetSecondaryColor(Detail::VehicleBackend::GetCurrent()); }
void SetPrimaryColor(int color) {
    Colors colors = GetColors();
    colors.primary = color;
    SetColors(colors);
}
void SetSecondaryColor(int color) {
    Colors colors = GetColors();
    colors.secondary = color;
    SetColors(colors);
}
int GetPaintjob() { return -1; }
bool SetPaintjob(int) { return false; }
void AddUpgrade(unsigned int) {}
void RemoveUpgrade(unsigned int) {}
void RemoveAllUpgrades() {}
int GetUpgrade(int) { return -1; }
void WarpToSeat(int seatIndex) { Detail::VehicleBackend::WarpToSeat(Detail::VehicleBackend::GetCurrent(), seatIndex); }
void OpenDoor(int doorIndex) { Detail::VehicleBackend::OpenDoor(Detail::VehicleBackend::GetCurrent(), doorIndex); }
void PopDoor(int) {}
void BlowUpAll() { Detail::VehicleBackend::BlowUpAll(); }
bool Spawn(unsigned int modelId, const SpawnOptions& options) {
    return Detail::VehicleBackend::Spawn(modelId, options.asDriver, options.aircraftInAir);
}
bool Spawn(unsigned int modelId) {
    return Spawn(modelId, SpawnOptions{});
}
void ApplySpeedLock(float speed) { s_speedLock = speed > 0.0f ? speed : 0.0f; }
void ApplyTargetSpeed(float speed) { s_targetSpeed = speed > 0.0f ? speed : 0.0f; }
void RestoreTargetSpeed() { s_targetSpeed = 0.0f; }


} // namespace XBase::Vehicle