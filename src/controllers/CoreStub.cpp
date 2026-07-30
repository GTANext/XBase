#include <XBase/Core.h>
#include <XBase/Player.h>

namespace XBase::Core {

bool s_gameInitialized = false;
DomainMask s_enabledDomains = 0;

void Init(DomainMask enabledDomains) {
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
    if (!s_gameInitialized) return;
    if (IsDomainEnabled(Domain::Player)) Player::Process();
}

void SetEnabledDomains(DomainMask enabledDomains) {
    constexpr DomainMask supportedDomains = DomainBit(Domain::Player);
    const DomainMask effectiveDomains = enabledDomains & supportedDomains;
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
    // 桩后端不访问游戏内存；宿主自己的 GameLogic 就绪检查仍是唯一门槛。
    return true;
}

void NotifyGameInit() {
    if (IsDomainEnabled(Domain::Player)) {
        Player::NotifyGameInit();
    }
    s_gameInitialized = true;
}

} // namespace XBase::Core