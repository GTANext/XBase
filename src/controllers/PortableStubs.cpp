#include <string>

#include <XBase/BulletAssist.h>
#include <XBase/Camera.h>
#include <XBase/Cheats.h>
#include <XBase/Hooks.h>
#include <XBase/Overlay.h>
#include <XBase/Ped.h>
#include <XBase/Scene.h>
#include <XBase/Theme.h>
#include <XBase/Types.h>
#include <XBase/UI.h>
#include <XBase/Vehicle.h>
#include <XBase/VehicleEffects.h>

#include <utility>

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
void BoatFly(bool) {}
bool IsBoatFly() { return false; }
void DriveWater(bool) {}
bool IsDriveWater() { return false; }
void TankMode(bool) {}
bool IsTankMode() { return false; }
void AimDrive(bool) {}
bool IsAimDrive() { return false; }
void NoDerail(bool) {}
bool IsNoDerail() { return false; }
void FlipNoBurn(bool) {}
bool IsFlipNoBurn() { return false; }
void StayOnBike(bool) {}
bool IsStayOnBike() { return false; }
void BikeFly(bool) {}
bool IsBikeFly() { return false; }
}

