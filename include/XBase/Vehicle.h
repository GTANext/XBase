#pragma once

#include "Types.h"

class CVehicle;

namespace XBase::Vehicle {

struct RuntimeOptions {
    bool noDamage = false;
    bool autoUnflip = false;
    bool heavy = false;
    bool watertight = false;
    bool speedLock = false;
    float speed = 60.0f;
};

struct Colors {
    int primary = 0;
    int secondary = 0;
    int tertiary = 0;
    int quaternary = 0;
};

enum class SpawnFailureReason {
    None,
    SpawnInProgress,
    RateLimited,
    InvalidModel,
    BackendRejected,
};

enum class VehicleEventType {
    SpawnRejected,
    Spawned,
    PreviousVehicleCleaned,
    PreviousVehicleCleanupSkipped,
};

struct SpawnOptions {
    bool asDriver = true;
    bool aircraftInAir = true;
    bool cleanupPrevious = true;
};

struct SpawnPolicy {
    unsigned int windowMs = 3000;
    unsigned int maxSpawns = 2;
};

struct SpawnResult {
    bool success = false;
    CVehicle* vehicle = nullptr;
    SpawnFailureReason failure = SpawnFailureReason::None;
};

struct VehicleEvent {
    VehicleEventType type = VehicleEventType::SpawnRejected;
    SpawnFailureReason reason = SpawnFailureReason::None;
    unsigned int modelId = 0;
};

CVehicle* GetCurrent();
void SetRuntimeOptions(const RuntimeOptions& options);
void SetTrafficDensity(float density);
void SetAutoDriveToWaypoint(bool enable);
void SetSpawnPolicy(const SpawnPolicy& policy);
SpawnPolicy GetSpawnPolicy();
SpawnResult SpawnEx(unsigned int modelId, const SpawnOptions& options);
bool PollEvent(VehicleEvent& event);
void NotifyGameInit();
void Process();
void Shutdown();

void Repair();
void Start();
void Stop();
void SetEngine(bool enable);
void Unflip();
void SetHeavy(bool enable);
void SetWatertight(bool enable);
float GetHealth();
void SetHealth(float health);
bool GetLights();
void SetLights(bool enable);
bool GetLocked();
void SetLocked(bool enable);
Types::ProofState GetProofState();
void SetProofState(const Types::ProofState& state);
bool GetVisible();
void SetVisible(bool enable);
#ifdef GTASA
bool GetAlwaysSkidMarks();
void SetAlwaysSkidMarks(bool enable);
bool GetDisableParticles();
void SetDisableParticles(bool enable);
bool GetDriverTargetable();
void SetDriverTargetable(bool enable);
bool GetHeatSeekingTargetable();
void SetHeatSeekingTargetable(bool enable);
bool GetPetrolTankWeakPoint();
void SetPetrolTankWeakPoint(bool enable);
bool GetSirenOrAlarm();
void SetSirenOrAlarm(bool enable);
bool GetTakeLessDamage();
void SetTakeLessDamage(bool enable);
#endif

#ifdef GTASA
void AddUpgrade(unsigned int modelId);
void RemoveUpgrade(unsigned int modelId);
void RemoveAllUpgrades();
int GetUpgrade(int slot);
#endif

Colors GetColors();
void SetColors(const Colors& colors);
int  GetPrimaryColor();
int  GetSecondaryColor();
void SetPrimaryColor(int color);
void SetSecondaryColor(int color);
#ifdef GTASA
int  GetPaintjob();
bool SetPaintjob(int paintjob);
#endif

void OpenDoor(int doorIndex);
void WarpToSeat(int seatIndex);
#ifdef GTASA
void PopDoor(int doorIndex);
#endif
void ApplySpeedLock(float speed);
void ApplyTargetSpeed(float speed);
void RestoreTargetSpeed();

void BlowUpAll();
bool Spawn(unsigned int modelId, const SpawnOptions& options);
bool Spawn(unsigned int modelId);

} // namespace XBase::Vehicle
