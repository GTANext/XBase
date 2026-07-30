#pragma once

#include "Types.h"

class CVehicle;

namespace XBase::Vehicle {

CVehicle* GetCurrent();
void Process();

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
bool GetAlwaysSkidMarks();
void SetAlwaysSkidMarks(bool enable);
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

void AddUpgrade(unsigned int modelId);
void RemoveUpgrade(unsigned int modelId);
void RemoveAllUpgrades();
int GetUpgrade(int slot);

int  GetPrimaryColor();
int  GetSecondaryColor();
void SetPrimaryColor(int color);
void SetSecondaryColor(int color);
int  GetPaintjob();
bool SetPaintjob(int paintjob);

void OpenDoor(int doorIndex);
void PopDoor(int doorIndex);
void WarpToSeat(int seatIndex);
void SetTrafficDensity(float density);
bool GetDisableParticles();
void SetDisableParticles(bool enable);
void ApplySpeedLock(float speed);
void ApplyTargetSpeed(float speed);
void RestoreTargetSpeed();

void BlowUpAll();
bool Spawn(unsigned int modelId);

} // namespace XBase::Vehicle
