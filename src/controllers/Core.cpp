#include <XBase/Core.h>

#include "CoreLifecycle.h"

#include "CWorld.h"

namespace XBase::Core {

bool s_gameInitialized = false;
DomainMask s_requestedDomains = 0;
DomainMask s_activeDomains = 0;

void Init(DomainMask enabledDomains) {
    Detail::SetEnabledDomains(s_requestedDomains, s_activeDomains, s_gameInitialized, enabledDomains);
}

void Shutdown() {
    Detail::Shutdown(s_requestedDomains, s_activeDomains, s_gameInitialized);
}

void Process() {
    Detail::Process(s_activeDomains, s_gameInitialized, IsWorldReady());
}

void SetEnabledDomains(DomainMask enabledDomains) {
    Detail::SetEnabledDomains(s_requestedDomains, s_activeDomains, s_gameInitialized, enabledDomains);
}

DomainMask GetEnabledDomains() {
    return s_activeDomains;
}

bool IsDomainEnabled(Domain domain) {
    return Detail::IsEnabled(s_activeDomains, domain);
}

bool IsWorldReady() {
    return CWorld::Players && CWorld::Players[CWorld::PlayerInFocus].m_pPed != nullptr;
}

void NotifyGameInit() {
    Detail::NotifyGameInit(s_activeDomains, s_gameInitialized);
}

} // namespace XBase::Core