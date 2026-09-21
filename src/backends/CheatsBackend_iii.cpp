#include "CheatsBackend.h"

#include "CVehicle.h"

#include <cstdint>

namespace XBase::Detail::CheatsBackend {
namespace {

bool s_flyingCars = false;
bool s_perfectHandling = false;

// 游戏在读档或脚本介入后可能重置作弊标志，每帧回写保持宿主设置
// 直接用 SDK 变量而不是裸地址，1.0 / 1.1 / Steam 都能落到正确位置
void Apply() {
    CVehicle::bAllDodosCheat = s_flyingCars;
    CVehicle::bCheat3 = s_perfectHandling;
}

} // namespace

bool Init() {
    return true;
}

void Shutdown() {
    s_flyingCars = false;
    s_perfectHandling = false;
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

void SetPerfectHandling(bool enable) {
    s_perfectHandling = enable;
    Apply();
}

bool IsPerfectHandling() {
    return s_perfectHandling;
}

void SetBoatFly(bool) {
}

bool IsBoatFly() {
    return false;
}

void SetDriveWater(bool) {
}

bool IsDriveWater() {
    return false;
}

void SetGreenLights(bool) {
}

bool IsGreenLights() {
    return false;
}

} // namespace XBase::Detail::CheatsBackend
