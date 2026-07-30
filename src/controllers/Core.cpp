#include <XBase/Core.h>
#include <XBase/Version.h>
#include <XBase/Player.h>
#include <XBase/Vehicle.h>
#include <XBase/World.h>
#include <XBase/Ped.h>
#include <XBase/Weapon.h>
#include <XBase/Teleport.h>
#include <XBase/Scene.h>
#include <XBase/Visual.h>
#include <XBase/BulletAssist.h>
#include <XBase/Overlay.h>
#include <XBase/Cheats.h>
#include "plugin.h"
#include "CWorld.h"

namespace XBase::Core {

bool s_gameInitialized = false;
DomainMask s_enabledDomains = 0;

void Init(DomainMask enabledDomains) {
    // 宿主可能先收到 initGame 再完成延迟初始化，不能在这里吞掉通知。
    SetEnabledDomains(enabledDomains);
}

void Shutdown() {
    if (IsDomainEnabled(Domain::Player)) {
        Player::Shutdown();
    }
    s_enabledDomains = 0;
    s_gameInitialized = false;
}

void Process() {
    if (!s_gameInitialized || !IsWorldReady()) return;

    if (IsDomainEnabled(Domain::Player)) Player::Process();
    if (IsDomainEnabled(Domain::Ped)) Ped::Process();
    if (IsDomainEnabled(Domain::Vehicle)) Vehicle::Process();
    if (IsDomainEnabled(Domain::World)) World::Process();
    if (IsDomainEnabled(Domain::Weapon)) Weapon::Process();
    if (IsDomainEnabled(Domain::Teleport)) Teleport::Process();
    if (IsDomainEnabled(Domain::Scene)) Scene::Process();
    if (IsDomainEnabled(Domain::Visual)) Visual::Process();
    if (IsDomainEnabled(Domain::BulletAssist)) BulletAssist::Process();
    if (IsDomainEnabled(Domain::Overlay)) Overlay::Process();
}

void SetEnabledDomains(DomainMask enabledDomains) {
    const DomainMask effectiveDomains = enabledDomains & AllDomains;
    const bool playerWasEnabled = IsDomainEnabled(Domain::Player);
    const bool playerWillBeEnabled = (effectiveDomains & DomainBit(Domain::Player)) != 0;
    if (playerWasEnabled && !playerWillBeEnabled) {
        Player::Shutdown();
    }
    s_enabledDomains = effectiveDomains;
    if (!playerWasEnabled && playerWillBeEnabled && s_gameInitialized) {
        Player::NotifyGameInit();
    }
}

DomainMask GetEnabledDomains() {
    return s_enabledDomains;
}

bool IsDomainEnabled(Domain domain) {
    return (s_enabledDomains & DomainBit(domain)) != 0;
}

bool IsWorldReady() {
    return CWorld::Players && CWorld::Players[CWorld::PlayerInFocus].m_pPed != nullptr;
}

void NotifyGameInit() {
    if (IsDomainEnabled(Domain::Player)) {
        Player::NotifyGameInit();
    }
    s_gameInitialized = true;
}

} // namespace XBase::Core
