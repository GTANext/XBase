#include <XBase/Capabilities.h>
#include <XBase/Version.h>

namespace XBase {

bool HasCapability(Capability capability) {
#if defined(XBASE_BACKEND_SA)
    switch (capability) {
    case Capability::Player:
    case Capability::Ped:
    case Capability::Vehicle:
    case Capability::World:
    case Capability::Weapon:
    case Capability::Scene:
    case Capability::Visual:
    case Capability::Teleport:
    case Capability::BulletAssist:
        return true;
    case Capability::Overlay:
    case Capability::Hooks:
    case Capability::Ui:
    default:
        return false;
    }
#elif defined(XBASE_BACKEND_VC) || defined(XBASE_BACKEND_III)
    return capability == Capability::Player;
#else
    (void)capability;
    return false;
#endif
}

bool HasCapability(FeatureCapability capability) {
#if defined(XBASE_BACKEND_SA)
    switch (capability) {
    case FeatureCapability::PlayerBasicState:
    case FeatureCapability::PlayerRuntimeEffects:
    case FeatureCapability::PlayerProofs:
    case FeatureCapability::PlayerMovement:
    case FeatureCapability::PlayerAppearance:
    case FeatureCapability::PlayerClothes:
    case FeatureCapability::PlayerStats:
    case FeatureCapability::PlayerCheats:
    case FeatureCapability::PlayerKeepStuff:
    case FeatureCapability::PlayerSaveGame:
    case FeatureCapability::PedBasic:
    case FeatureCapability::PedMarkerSpawn:
    case FeatureCapability::VehicleBasic:
    case FeatureCapability::VehicleColors:
    case FeatureCapability::VehiclePaintjob:
    case FeatureCapability::VehicleUpgrades:
    case FeatureCapability::WorldTime:
    case FeatureCapability::WorldWeather:
    case FeatureCapability::WorldGravity:
    case FeatureCapability::WorldPickups:
        return true;
    default:
        return false;
    }
#elif defined(XBASE_BACKEND_VC) || defined(XBASE_BACKEND_III)
    switch (capability) {
    case FeatureCapability::PlayerBasicState:
    case FeatureCapability::PlayerRuntimeEffects:
    case FeatureCapability::PlayerProofs:
    case FeatureCapability::PlayerMovement:
        return true;
    default:
        return false;
    }
#else
    (void)capability;
    return false;
#endif
}

} // namespace XBase