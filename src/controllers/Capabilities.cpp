#include <XBase/Capabilities.h>

namespace XBase {

CapabilitySupport GetCapabilitySupport(Capability capability) {
#if defined(XBASE_BACKEND_SA)
    switch (capability) {
    case Capability::Player:
    case Capability::Vehicle:
    case Capability::Weapon:
    case Capability::Visual:
    case Capability::Teleport:
    case Capability::Hooks:
    case Capability::Ui:
        return CapabilitySupport::Supported;
    case Capability::Ped:
    case Capability::World:
    case Capability::Scene:
    case Capability::Overlay:
    case Capability::BulletAssist:
        return CapabilitySupport::Partial;
    }
#elif defined(XBASE_BACKEND_VC)
    switch (capability) {
    case Capability::Player:
    case Capability::Ped:
    case Capability::Vehicle:
        return CapabilitySupport::Partial;
    case Capability::Hooks:
    case Capability::Ui:
        return CapabilitySupport::Supported;
    default:
        return CapabilitySupport::Unsupported;
    }
#elif defined(XBASE_BACKEND_III)
    switch (capability) {
    case Capability::Player:
    case Capability::Ped:
    case Capability::Vehicle:
        return CapabilitySupport::Partial;
    case Capability::Hooks:
    case Capability::Ui:
        return CapabilitySupport::Supported;
    default:
        return CapabilitySupport::Unsupported;
    }
#else
    (void)capability;
#endif
    return CapabilitySupport::Unsupported;
}

CapabilitySupport GetCapabilitySupport(FeatureCapability capability) {
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
    case FeatureCapability::PedSpawn:
    case FeatureCapability::PedDelete:
    case FeatureCapability::PedAttributes:
    case FeatureCapability::PedClassification:
    case FeatureCapability::VehicleBasic:
    case FeatureCapability::VehicleColors:
    case FeatureCapability::VehicleDoors:
    case FeatureCapability::VehicleSpawn:
    case FeatureCapability::VehicleSpawnSession:
    case FeatureCapability::VehicleDelete:
    case FeatureCapability::VehicleEvents:
    case FeatureCapability::VehiclePopDoors:
    case FeatureCapability::VehicleSpecialAttributes:
    case FeatureCapability::VehiclePaintjob:
    case FeatureCapability::VehicleUpgrades:
    case FeatureCapability::WorldTime:
    case FeatureCapability::WorldWeather:
    case FeatureCapability::WorldGravity:
    case FeatureCapability::WeaponBasic:
    case FeatureCapability::WeaponGive:
    case FeatureCapability::WeaponDrop:
    case FeatureCapability::WeaponSkills:
    case FeatureCapability::WeaponRuntimeEffects:
    case FeatureCapability::SceneAnimation:
    case FeatureCapability::SceneMission:
    case FeatureCapability::TeleportBasic:
    case FeatureCapability::VisualHudRadar:
    case FeatureCapability::VisualFilter:
        return CapabilitySupport::Supported;
    case FeatureCapability::PedMarkerSpawn:
    case FeatureCapability::PedGlobalStrategies:
    case FeatureCapability::WorldPickups:
    case FeatureCapability::SceneParticle:
    case FeatureCapability::SceneCutscene:
    case FeatureCapability::BulletAssistFireSuppression:
        return CapabilitySupport::Partial;
    }
#elif defined(XBASE_BACKEND_VC)
    switch (capability) {
    case FeatureCapability::PlayerBasicState:
    case FeatureCapability::PlayerProofs:
    case FeatureCapability::PlayerMovement:
    case FeatureCapability::PedBasic:
    case FeatureCapability::PedSpawn:
    case FeatureCapability::PedDelete:
    case FeatureCapability::PedAttributes:
    case FeatureCapability::PedClassification:
    case FeatureCapability::VehicleDoors:
    case FeatureCapability::VehicleSpawn:
    case FeatureCapability::VehicleSpawnSession:
    case FeatureCapability::VehicleDelete:
    case FeatureCapability::VehicleEvents:
        return CapabilitySupport::Supported;
    case FeatureCapability::PlayerRuntimeEffects:
    case FeatureCapability::VehicleBasic:
    case FeatureCapability::VehicleColors:
        return CapabilitySupport::Partial;
    default:
        return CapabilitySupport::Unsupported;
    }
#elif defined(XBASE_BACKEND_III)
    switch (capability) {
    case FeatureCapability::PlayerBasicState:
    case FeatureCapability::PlayerProofs:
    case FeatureCapability::PlayerMovement:
    case FeatureCapability::PedBasic:
    case FeatureCapability::PedSpawn:
    case FeatureCapability::PedDelete:
    case FeatureCapability::PedAttributes:
    case FeatureCapability::PedClassification:
    case FeatureCapability::VehicleDoors:
    case FeatureCapability::VehicleSpawn:
    case FeatureCapability::VehicleSpawnSession:
    case FeatureCapability::VehicleDelete:
    case FeatureCapability::VehicleEvents:
        return CapabilitySupport::Supported;
    case FeatureCapability::PlayerRuntimeEffects:
    case FeatureCapability::VehicleBasic:
    case FeatureCapability::VehicleColors:
        return CapabilitySupport::Partial;
    default:
        return CapabilitySupport::Unsupported;
    }
#else
    (void)capability;
#endif
    return CapabilitySupport::Unsupported;
}

bool HasCapability(Capability capability) {
    return GetCapabilitySupport(capability) != CapabilitySupport::Unsupported;
}

bool HasCapability(FeatureCapability capability) {
    return GetCapabilitySupport(capability) != CapabilitySupport::Unsupported;
}

} // namespace XBase