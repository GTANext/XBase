#include <string>

#include <XBase/BulletAssist.h>
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
#include <XBase/Visual.h>
#include <XBase/Weapon.h>
#include <XBase/World.h>

#include <utility>

namespace XBase::World {
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
void Process() {}
void GiveAll() {}
void ClearAll() {}
void DropWeapon() {}
void DropCurrent() {}
int RemoveTrackedPickups() { return 0; }
void Give(unsigned int, unsigned int) {}
void GiveModel(unsigned int, unsigned int) {}
void MaxWeaponSkills() {}
void SetInfiniteAmmo(bool) {}
void SetFastReload(bool) {}
void ResetStats() {}
}

namespace XBase::Teleport {
CVector GetCurrentPosition() { return CVector(0.0f, 0.0f, 10.0f); }
void To(float, float, float, int) {}
void Forward(float) {}
void MapPosition(float, float, bool) {}
void Marker(bool) {}
void Center() {}
void Process() {}
}

namespace XBase::Scene {
void Process() {}
void Shutdown() {}
bool PlayAnimation(const char*, const char*, bool) { return false; }
void StopAnimation() {}
bool PlayParticle(const char*) { return false; }
void RemoveAllParticles() {}
void RemoveLatestParticle() {}
bool StartCutscene(const char*) { return false; }
void StopCutscene() {}
bool IsCutsceneRunning() { return false; }
const char* GetMissionStatus() { return "unsupported"; }
void FailMission() {}
void StartMission(int) {}
void SetFightingStyle(int) {}
void SetWalkingStyle(int) {}
}

namespace XBase::Visual {
void DisplayHud(bool) {}
void DisplayRadar(bool) {}
void SetFilter(int, float) {}
void Process() {}
}

namespace XBase::BulletAssist {
void Init() {}
void Process() {}
void Draw() {}
bool ShouldSuppressPedFire(CPed* ped) { return ped != nullptr; }
}

namespace XBase::Overlay {
void Init() {}
void Process() {}
void Draw() {}
void SetVisible(bool) {}
bool IsVisible() { return false; }
void Toggle() {}
void SetPosition(bool, bool, bool, bool) {}
}

namespace XBase::Cheats {
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

namespace XBase::Hooks {
bool Init() { return false; }
void Shutdown() {}
bool IsInitialized() { return false; }
bool IsReady() { return false; }
void SetDrawCallback(std::function<void()>) {}
void SetMenuVisible(bool) {}
bool IsMenuVisible() { return false; }
void ToggleMenu() {}
LPDIRECT3DDEVICE9 GetDevice() { return nullptr; }
}

namespace XBase::Theme {
void Init() {}
void ApplyPreset(Preset) {}
void ApplyCustom(const ColorSet&) {}
ColorSet GetColors() { return {}; }
void PushStyle() {}
void PopStyle() {}
void LoadFont(const char*, float) {}
ImFont* GetDefaultFont() { return nullptr; }
}

namespace XBase::UI {
void Init(const std::string&) {}
void Process() {}
void Shutdown() {}
void AddWindow(const std::string&, std::function<void()>, bool, ImGuiWindowFlags) {}
void RemoveWindow(const std::string&) {}
void SetWindowVisible(const std::string&, bool) {}
bool IsWindowVisible(const std::string&) { return false; }
void ToggleWindow(const std::string&) {}
void BeginTabBar(const std::string&) {}
void AddTab(const std::string&, std::function<void()>) {}
bool RenderTabBar(float) { return false; }
void EndTabBar() {}
void CenterText(const char*) {}
void SeparatorText(const char*) {}
bool StyledButton(const char*, ImVec2) { return false; }
void BeginGroupBox(const char*, ImVec2) {}
void EndGroupBox() {}
void HelpMarker(const char*) {}
}