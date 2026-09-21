#include <XBase/Cheats.h>

#include "../backends/CheatsBackend.h"

#include <algorithm>

namespace {

XBase::Cheats::RandomSettings s_randomSettings;

} // namespace

namespace XBase::Cheats {

void SetRandomSettings(const RandomSettings& settings) {
    s_randomSettings = settings;
    s_randomSettings.intervalSeconds = std::clamp(s_randomSettings.intervalSeconds, 1, 60);
}

RandomSettings GetRandomSettings() {
    return s_randomSettings;
}

std::size_t GetRandomCheatCount() {
    return 0;
}

const char* GetRandomCheatName(std::size_t) {
    return nullptr;
}

bool IsRandomCheatEnabled(std::size_t) {
    return false;
}

bool SetRandomCheatEnabled(std::size_t, bool) {
    return false;
}

void Init() {
    Detail::CheatsBackend::Init();
}

void NotifyGameInit() {
    Shutdown();
}

void Process() {
    Detail::CheatsBackend::Process();
}

void Shutdown() {
    Detail::CheatsBackend::Shutdown();
    s_randomSettings = {};
}

void FlyingCars(bool enable) {
    Detail::CheatsBackend::SetFlyingCars(enable);
}

bool IsFlyingCars() {
    return Detail::CheatsBackend::IsFlyingCars();
}

void AllCarsHaveNitro(bool) {
}

bool IsAllCarsHaveNitro() {
    return false;
}

void PerfectHandling(bool enable) {
    Detail::CheatsBackend::SetPerfectHandling(enable);
}

bool IsPerfectHandling() {
    return Detail::CheatsBackend::IsPerfectHandling();
}

void GreenLights(bool enable) {
    Detail::CheatsBackend::SetGreenLights(enable);
}

bool IsGreenLights() {
    return Detail::CheatsBackend::IsGreenLights();
}

void Riot(bool) {
}

bool IsRiot() {
    return false;
}

void BoatFly(bool enable) {
    Detail::CheatsBackend::SetBoatFly(enable);
}

bool IsBoatFly() {
    return Detail::CheatsBackend::IsBoatFly();
}

void DriveWater(bool enable) {
    Detail::CheatsBackend::SetDriveWater(enable);
}

bool IsDriveWater() {
    return Detail::CheatsBackend::IsDriveWater();
}

void TankMode(bool) {
}

bool IsTankMode() {
    return false;
}

void AimDrive(bool) {
}

bool IsAimDrive() {
    return false;
}

void NoDerail(bool) {
}

bool IsNoDerail() {
    return false;
}

void FlipNoBurn(bool) {
}

bool IsFlipNoBurn() {
    return false;
}

void StayOnBike(bool) {
}

bool IsStayOnBike() {
    return false;
}

void BikeFly(bool) {
}

bool IsBikeFly() {
    return false;
}

} // namespace XBase::Cheats
