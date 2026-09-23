#pragma once

namespace XBase::Detail::CheatsBackend {

bool Init();
void Shutdown();
void Process();

// 各版本按可用地址实现，不支持的动作保持空操作并返回假
void SetFlyingCars(bool enable);
bool IsFlyingCars();
void SetBoatFly(bool enable);
bool IsBoatFly();
void SetDriveWater(bool enable);
bool IsDriveWater();
void SetGreenLights(bool enable);
bool IsGreenLights();
void SetPerfectHandling(bool enable);
bool IsPerfectHandling();

} // namespace XBase::Detail::CheatsBackend
