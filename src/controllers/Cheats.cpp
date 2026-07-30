#include <XBase/Cheats.h>

namespace {

enum CheatIndex {
    CHEAT_PERFECT_HANDLING = 28,
    CHEAT_BGREEN_LIGHTS    = 30,
    CHEAT_CARS_FLY         = 48,
    CHEAT_ALL_CARS_NITRO   = 53,
    CHEAT_RIOT             = 69,
};

bool* CheatAddr(int index) {
    return reinterpret_cast<bool*>(0x969130 + index);
}

} // namespace

namespace XBase::Cheats {

void FlyingCars(bool enable) {
    *CheatAddr(CHEAT_CARS_FLY) = enable;
}

bool IsFlyingCars() {
    return *CheatAddr(CHEAT_CARS_FLY);
}

void AllCarsHaveNitro(bool enable) {
    *CheatAddr(CHEAT_ALL_CARS_NITRO) = enable;
}

bool IsAllCarsHaveNitro() {
    return *CheatAddr(CHEAT_ALL_CARS_NITRO);
}

void PerfectHandling(bool enable) {
    *CheatAddr(CHEAT_PERFECT_HANDLING) = enable;
}

bool IsPerfectHandling() {
    return *CheatAddr(CHEAT_PERFECT_HANDLING);
}

void GreenLights(bool enable) {
    *CheatAddr(CHEAT_BGREEN_LIGHTS) = enable;
}

bool IsGreenLights() {
    return *CheatAddr(CHEAT_BGREEN_LIGHTS);
}

void Riot(bool enable) {
    *CheatAddr(CHEAT_RIOT) = enable;
}

bool IsRiot() {
    return *CheatAddr(CHEAT_RIOT);
}

} // namespace XBase::Cheats
