#include <string>

#include <XBase/BulletAssist.h>
#include <XBase/Camera.h>
#include <XBase/Cheats.h>
#include <XBase/Hooks.h>
#include <XBase/Overlay.h>
#include <XBase/Ped.h>
#include <XBase/Scene.h>
#include <XBase/Teleport.h>
#include <XBase/Theme.h>
#include <XBase/Types.h>
#include <XBase/UI.h>
#include <XBase/Vehicle.h>
#include <XBase/VehicleEffects.h>
#include <XBase/Visual.h>
#include <XBase/Weapon.h>
#include <XBase/World.h>

#include <utility>

namespace XBase::World {
void NotifyGameInit() {}
void Shutdown() {}
void Process() {}
int GetWeather() { return 0; }
void SetWeather(int, bool) {}
void ReleaseWeather() {}
bool IsWeatherLocked() { return false; }
void SetTime(int, int) {}
void GetTime(int& hour, int& minute) { hour = 0; minute = 0; }
void SyncTimeWithSystemClock() {}
void SetGameSpeed(float) {}
float GetGameSpeed() { return 0.0f; }
int GetFpsLimit() { return 0; }
void SetFpsLimit(int) {}
float GetGravity() { return 0.0f; }
void SetGravity(float) {}
float GetRain() { return 0.0f; }
void SetRain(float) {}
float GetFoggyness() { return 0.0f; }
void SetFoggyness(float) {}
float GetCloudCoverage() { return 0.0f; }
void SetCloudCoverage(float) {}
float GetWind() { return 0.0f; }
void SetWind(float) {}
float GetSandstorm() { return 0.0f; }
void SetSandstorm(float) {}
float GetExtraSunnyness() { return 0.0f; }
void SetExtraSunnyness(float) {}
float GetWetRoads() { return 0.0f; }
void SetWetRoads(float) {}
void DestroyAllVehicles() {}
void DestroyAllPeds() {}
void SetFreezeTime(bool) {}
bool IsTimeFrozen() { return false; }
void SetLockedTime(bool, int, int) {}
void SetFasterClock(bool) {}
bool IsFasterClock() { return false; }
void SetDisableReplay(bool) {}
bool IsReplayDisabled() { return false; }
void SetDisableCheats(bool) {}
bool AreCheatsDisabled() { return false; }
void SetForbiddenAreaWanted(bool) {}
bool IsForbiddenAreaWanted() { return false; }
void SetFreePayNSpray(bool) {}
bool IsFreePayNSpray() { return false; }
void SetNoWaterPhysics(bool) {}
bool IsNoWaterPhysics() { return false; }
int GetDaysPassed() { return 0; }
void SetDaysPassed(int) {}
int SpawnPickup(const Types::PickupOptions&) { return -1; }
bool UpdateLastPickup(const Types::PickupOptions&) { return false; }
bool RemoveLastPickup() { return false; }
bool RemoveTrackedPickups() { return false; }
}

namespace XBase::Weapon {
void NotifyGameInit() {}
void Shutdown() {}
void Process() {}
bool GiveAll() { return false; }
bool ClearAll() { return false; }
bool DropWeapon() { return false; }
bool DropCurrent() { return false; }
int RemoveTrackedPickups() { return 0; }
bool Give(unsigned int, unsigned int) { return false; }
bool GiveModel(unsigned int, unsigned int) { return false; }
bool MaxWeaponSkills() { return false; }
bool SetInfiniteAmmo(bool) { return false; }
bool SetFastReload(bool) { return false; }
bool ResetStats() { return false; }
}

namespace XBase::Teleport {
bool TryGetCurrentPosition(Vec3& position) { position = {}; return false; }
Vec3 GetCurrentPosition() { return {}; }
bool To(float, float, float, int) { return false; }
bool Forward(float) { return false; }
bool MapPosition(float, float, bool) { return false; }
bool Marker(bool) { return false; }
bool Center() { return false; }
void Process() {}
}

namespace XBase::Scene {
void Process() {}
void NotifyGameInit() {}
void Shutdown() {}
bool PlayAnimation(const char*, const char*, bool) { return false; }
bool StopAnimation() { return false; }
bool PlayParticle(const char*) { return false; }
bool RemoveAllParticles() { return false; }
bool RemoveLatestParticle() { return false; }
bool StartCutscene(const char*) { return false; }
bool StartCutscene(const char*, int) { return false; }
bool StopCutscene() { return false; }
bool IsCutsceneRunning() { return false; }
const char* GetMissionStatus() { return "unsupported"; }
bool FailMission() { return false; }
bool StartMission(int) { return false; }
bool SetFightingStyle(int) { return false; }
bool SetWalkingStyle(int) { return false; }
}

namespace XBase::VehicleEffects {
bool ApplyCurrentNeon(const NeonSettings&) { return false; }
void Init() {}
void NotifyGameInit() {}
void Process() {}
void Shutdown() {}
}

namespace XBase::Visual {
void NotifyGameInit() {}
void Shutdown() {}
bool DisplayHud(bool) { return false; }
bool DisplayRadar(bool) { return false; }
bool SetFilter(int, float) { return false; }
void Process() {}
}

namespace XBase::Camera {
bool SetMode(Mode mode) { return mode == Mode::Disabled; }
Mode GetMode() { return Mode::Disabled; }
bool IsActive() { return false; }
void SetSettings(const Settings&) {}
Settings GetSettings() { return {}; }
void NotifyGameInit() {}
void Process() {}
void Shutdown() {}
}

namespace XBase::Overlay {
void Init() {}
void Process() {}
void Shutdown() {}
void Draw() {}
void SetVisible(bool) {}
bool IsVisible() { return false; }
void Toggle() {}
void SetPosition(bool, bool, bool, bool) {}
}

namespace XBase::Cheats {
void SetRandomSettings(const RandomSettings&) {}
RandomSettings GetRandomSettings() { return {}; }
std::size_t GetRandomCheatCount() { return 0; }
const char* GetRandomCheatName(std::size_t) { return nullptr; }
bool IsRandomCheatEnabled(std::size_t) { return false; }
bool SetRandomCheatEnabled(std::size_t, bool) { return false; }
void Init() {}
void NotifyGameInit() {}
void Process() {}
void Shutdown() {}
void FlyingCars(bool) {}
bool IsFlyingCars() { return false; }
void AllCarsHaveNitro(bool) {}
bool IsAllCarsHaveNitro() { return false; }
void PerfectHandling(bool) {}
bool IsPerfectHandling() { return false; }
void GreenLights(bool) {}
bool IsGreenLights() { return false; }
void Riot(bool) {}
bool IsRiot() { return false; }
}

