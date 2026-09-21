#include "CheatsBackend.h"

#include <cstdint>

namespace XBase::Detail::CheatsBackend {
namespace {

constexpr std::uintptr_t kFlyingCarsAddress = 0xA10B28;
constexpr std::uintptr_t kBoatFlyAddress = 0xA10B11;
constexpr std::uintptr_t kDriveWaterAddress = 0xA10B81;
constexpr std::uintptr_t kGreenLightsAddress = 0xA10ADC;

bool s_flyingCars = false;
bool s_boatFly = false;
bool s_driveWater = false;
bool s_greenLights = false;

// 游戏在读档或脚本介入后可能重置作弊标志，每帧回写保持宿主设置
void Apply() {
    *reinterpret_cast<bool*>(kFlyingCarsAddress) = s_flyingCars;
    *reinterpret_cast<bool*>(kBoatFlyAddress) = s_boatFly;
    *reinterpret_cast<bool*>(kDriveWaterAddress) = s_driveWater;
    *reinterpret_cast<bool*>(kGreenLightsAddress) = s_greenLights;
}

} // namespace

bool Init() {
    return true;
}

void Shutdown() {
    s_flyingCars = false;
    s_boatFly = false;
    s_driveWater = false;
    s_greenLights = false;
    Apply();
}

void Process() {
    Apply();
}

void SetFlyingCars(bool enable) {
    s_flyingCars = enable;
    Apply();
}

bool IsFlyingCars() {
    return s_flyingCars;
}

void SetBoatFly(bool enable) {
    s_boatFly = enable;
    Apply();
}

bool IsBoatFly() {
    return s_boatFly;
}

void SetDriveWater(bool enable) {
    s_driveWater = enable;
    Apply();
}

bool IsDriveWater() {
    return s_driveWater;
}

void SetGreenLights(bool enable) {
    s_greenLights = enable;
    Apply();
}

bool IsGreenLights() {
    return s_greenLights;
}

void SetPerfectHandling(bool) {
}

bool IsPerfectHandling() {
    return false;
}

} // namespace XBase::Detail::CheatsBackend
