#pragma once

#include <XBase/Core.h>
#include <XBase/Capabilities.h>
#include <XBase/Player.h>
#include <XBase/Ped.h>
#include <XBase/Vehicle.h>
#include <XBase/World.h>
#include <XBase/Weapon.h>
#include <XBase/Teleport.h>
#include <XBase/Scene.h>
#include <XBase/Visual.h>
#include <XBase/BulletAssist.h>
#include <XBase/Overlay.h>

#include <cstddef>
#include <iterator>

namespace XBase::Core::Detail {

using LifecycleFn = void (*)();

inline void Noop() {}

struct DomainLifecycle {
    Domain domain;
    LifecycleFn init;
    LifecycleFn notifyGameInit;
    LifecycleFn process;
    LifecycleFn shutdown;
};

inline const DomainLifecycle kDomains[] = {
    {Domain::Player,       Noop,               Player::NotifyGameInit,  Player::Process,       Player::Shutdown},
    {Domain::Ped,          Noop,               Ped::NotifyGameInit,     Ped::Process,          Ped::Shutdown},
    {Domain::Vehicle,      Noop,               Vehicle::NotifyGameInit, Vehicle::Process,      Vehicle::Shutdown},
    {Domain::World,        Noop,               World::NotifyGameInit,   World::Process,         World::Shutdown},
    {Domain::Weapon,       Noop,               Weapon::NotifyGameInit,  Weapon::Process,        Weapon::Shutdown},
    {Domain::Teleport,     Noop,               Noop,                    Teleport::Process,      Noop},
    {Domain::Scene,        Noop,               Scene::NotifyGameInit,   Scene::Process,         Scene::Shutdown},
    {Domain::Visual,       Noop,               Visual::NotifyGameInit,  Visual::Process,        Visual::Shutdown},
    {Domain::BulletAssist, BulletAssist::Init, Noop,                    BulletAssist::Process, BulletAssist::Shutdown},
    {Domain::Overlay,      Overlay::Init,      Noop,                    Overlay::Process,      Overlay::Shutdown},
};

inline bool IsEnabled(DomainMask mask, Domain domain) {
    return (mask & DomainBit(domain)) != 0;
}

inline Capability DomainCapability(Domain domain) {
    switch (domain) {
    case Domain::Player: return Capability::Player;
    case Domain::Ped: return Capability::Ped;
    case Domain::Vehicle: return Capability::Vehicle;
    case Domain::World: return Capability::World;
    case Domain::Weapon: return Capability::Weapon;
    case Domain::Teleport: return Capability::Teleport;
    case Domain::Scene: return Capability::Scene;
    case Domain::Visual: return Capability::Visual;
    case Domain::BulletAssist: return Capability::BulletAssist;
    case Domain::Overlay: return Capability::Overlay;
    }
    return Capability::Player;
}

inline DomainMask SupportedDomains(DomainMask requested) {
    DomainMask supported = 0;
    for (const DomainLifecycle& entry : kDomains) {
        if (IsEnabled(requested, entry.domain) && HasCapability(DomainCapability(entry.domain))) {
            supported |= DomainBit(entry.domain);
        }
    }
    return supported;
}

inline void SetEnabledDomains(DomainMask& current, bool gameInitialized, DomainMask requested) {
    const DomainMask next = SupportedDomains(requested & AllDomains);

    for (std::size_t i = std::size(kDomains); i > 0; --i) {
        const DomainLifecycle& entry = kDomains[i - 1];
        if (IsEnabled(current, entry.domain) && !IsEnabled(next, entry.domain)) entry.shutdown();
    }

    const DomainMask previous = current;
    current = next;
    for (const DomainLifecycle& entry : kDomains) {
        if (IsEnabled(previous, entry.domain) || !IsEnabled(next, entry.domain)) continue;
        entry.init();
        if (gameInitialized) entry.notifyGameInit();
    }
}

inline void NotifyGameInit(DomainMask enabled, bool& gameInitialized) {
    gameInitialized = true;
    for (const DomainLifecycle& entry : kDomains) {
        if (IsEnabled(enabled, entry.domain)) entry.notifyGameInit();
    }
}

inline void Process(DomainMask enabled, bool gameInitialized, bool worldReady) {
    if (!gameInitialized || !worldReady) return;
    for (const DomainLifecycle& entry : kDomains) {
        if (IsEnabled(enabled, entry.domain)) entry.process();
    }
}

inline void Shutdown(DomainMask& enabled, bool& gameInitialized) {
    for (std::size_t i = std::size(kDomains); i > 0; --i) {
        const DomainLifecycle& entry = kDomains[i - 1];
        if (IsEnabled(enabled, entry.domain)) entry.shutdown();
    }
    enabled = 0;
    gameInitialized = false;
}

} // namespace XBase::Core::Detail