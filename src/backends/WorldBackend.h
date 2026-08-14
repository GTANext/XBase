#pragma once

#include <XBase/Types.h>

namespace XBase::Detail::WorldBackend {

void GetTime(int& hour, int& minute);
void SetGameClock(int hour, int minute);
short GetOldWeather();
void ForceWeather(short weather);
void ReleaseWeather();
float GetTimeScale();
void SetTimeScale(float speed);
int GetMaxFps();
void SetMaxFps(int limit);
float GetGravity();
void SetGravity(float gravity);
int GetDaysPassed();
void SetDaysPassed(int days);
void DestroyAllVehicles();
void DestroyAllPeds();
bool IsReplayDisabled();
void SetDisableReplay(bool enable);
bool AreCheatsDisabled();
void SetDisableCheats(bool enable);
void SetFastClock(bool enable);
int SpawnPickupNearPlayer(const Types::PickupOptions& options, float& x, float& y, float& z);
int SpawnPickupAt(const Types::PickupOptions& options, float x, float y, float z);
void RemovePickup(int handle);

} // namespace XBase::Detail::WorldBackend