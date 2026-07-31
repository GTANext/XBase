#pragma once

#include <XBase/Types.h>

namespace XBase::Detail::VehicleBackend {

void* GetCurrent();
bool IsValidModel(unsigned int modelId);
bool IsValid(void* vehicle);
bool IsPlayerUsing(void* vehicle);
bool Delete(void* vehicle);
void Repair(void* vehicle);
void Start(void* vehicle);
void Stop(void* vehicle);
void SetEngine(void* vehicle, bool enable);
void Unflip(void* vehicle);
void SetHeavy(void* vehicle, bool enable);
void SetWatertight(void* vehicle, bool enable);
float GetHealth(void* vehicle);
void SetHealth(void* vehicle, float health);
bool GetLights(void* vehicle);
void SetLights(void* vehicle, bool enable);
bool GetLocked(void* vehicle);
void SetLocked(void* vehicle, bool enable);
Types::ProofState GetProofState(void* vehicle);
void SetProofState(void* vehicle, const Types::ProofState& state);
bool GetVisible(void* vehicle);
void SetVisible(void* vehicle, bool enable);
bool IsUpsideDown(void* vehicle);
#ifdef XBASE_BACKEND_SA
bool GetAlwaysSkidMarks(void* vehicle);
void SetAlwaysSkidMarks(void* vehicle, bool enable);
bool GetDisableParticles(void* vehicle);
void SetDisableParticles(void* vehicle, bool enable);
bool GetDriverTargetable(void* vehicle);
void SetDriverTargetable(void* vehicle, bool enable);
bool GetHeatSeekingTargetable(void* vehicle);
void SetHeatSeekingTargetable(void* vehicle, bool enable);
bool GetPetrolTankWeakPoint(void* vehicle);
void SetPetrolTankWeakPoint(void* vehicle, bool enable);
bool GetSirenOrAlarm(void* vehicle);
void SetSirenOrAlarm(void* vehicle, bool enable);
bool GetTakeLessDamage(void* vehicle);
void SetTakeLessDamage(void* vehicle, bool enable);
#endif
int GetPrimaryColor(void* vehicle);
int GetSecondaryColor(void* vehicle);
void SetColors(void* vehicle, int primary, int secondary, int tertiary, int quaternary);
void WarpToSeat(void* vehicle, int seatIndex);
void OpenDoor(void* vehicle, int doorIndex);
#ifdef XBASE_BACKEND_SA
void PopDoor(void* vehicle, int doorIndex);
#endif
void BlowUpAll();
bool Spawn(unsigned int modelId, bool asDriver, bool aircraftInAir);
void ApplySpeed(void* vehicle, float speed);

} // namespace XBase::Detail::VehicleBackend