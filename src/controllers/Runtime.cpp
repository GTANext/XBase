#include <XBase/Runtime.h>

#include "GameVersion.h"
#include "plugin.h"

#include <windows.h>

namespace XBase::Runtime {
namespace {

bool IsSupportedGameVersion() {
    const unsigned int version = plugin::GetGameVersion();
#if defined(XBASE_BACKEND_SA)
    return version == GAME_10US_HOODLUM || version == GAME_10US_COMPACT;
#elif defined(XBASE_BACKEND_VC) || defined(XBASE_BACKEND_III)
    return version == GAME_10EN;
#else
    (void)version;
    return false;
#endif
}

bool IsOnlineRuntimeLoaded() {
#if defined(XBASE_BACKEND_SA)
    return GetModuleHandleA("SAMP.dll") != nullptr
        || GetModuleHandleA("SAMP.asi") != nullptr;
#elif defined(XBASE_BACKEND_VC)
    return GetModuleHandleA("vcmp-proxy.dll") != nullptr
        || GetModuleHandleA("vcmp-proxy.asi") != nullptr;
#else
    return false;
#endif
}

} // namespace

GameTarget GetGameTarget() {
#if defined(XBASE_BACKEND_SA)
    return GameTarget::SanAndreas;
#elif defined(XBASE_BACKEND_VC)
    return GameTarget::ViceCity;
#elif defined(XBASE_BACKEND_III)
    return GameTarget::III;
#else
    return GameTarget::Unknown;
#endif
}

const char* GetGameKey() {
    switch (GetGameTarget()) {
    case GameTarget::SanAndreas:
        return "sa";
    case GameTarget::ViceCity:
        return "vc";
    case GameTarget::III:
        return "iii";
    case GameTarget::Unknown:
    default:
        return "unknown";
    }
}

const char* GetGameName() {
    switch (GetGameTarget()) {
    case GameTarget::SanAndreas:
        return "GTA San Andreas";
    case GameTarget::ViceCity:
        return "GTA Vice City";
    case GameTarget::III:
        return "GTA III";
    case GameTarget::Unknown:
    default:
        return "Unknown";
    }
}

ValidationResult ValidateEnvironment() {
    const GameTarget target = GetGameTarget();
    if (target == GameTarget::Unknown || !IsSupportedGameVersion()) {
        return {
            false,
            target,
            ValidationFailure::UnsupportedGameVersion,
            "unsupported game version",
        };
    }
    if (IsOnlineRuntimeLoaded()) {
        return {
            false,
            target,
            ValidationFailure::OnlineRuntimeDetected,
            "online multiplayer runtime detected",
        };
    }
    return {true, target, ValidationFailure::None, "runtime available"};
}

} // namespace XBase::Runtime