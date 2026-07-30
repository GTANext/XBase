#pragma once

namespace XBase {

enum class Capability {
    Player,
    Ped,
    Vehicle,
    World,
    Weapon,
    Scene,
    Visual,
    Teleport,
    Overlay,
    BulletAssist,
    Hooks,
    Ui,
};

enum class FeatureCapability {
    PlayerBasicState,
    PlayerRuntimeEffects,
    PlayerProofs,
    PlayerMovement,
    PlayerAppearance,
    PlayerClothes,
    PlayerStats,
    PlayerCheats,
    PlayerKeepStuff,
    PlayerSaveGame,
    PedBasic,
    PedMarkerSpawn,
    VehicleBasic,
    VehicleColors,
    VehiclePaintjob,
    VehicleUpgrades,
    WorldTime,
    WorldWeather,
    WorldGravity,
    WorldPickups,
};

// Domain queries are retained for compatibility. New integrations should use
// feature queries so a partially implemented backend is never over-reported.
bool HasCapability(Capability capability);
bool HasCapability(FeatureCapability capability);

} // namespace XBase