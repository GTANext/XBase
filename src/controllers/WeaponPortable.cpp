#include <XBase/Weapon.h>
#include <XBase/Core.h>

#include "../backends/WeaponBackend.h"

namespace XBase::Weapon {

void NotifyGameInit() {
    Detail::WeaponBackend::Init();
}

void Shutdown() {
    Detail::WeaponBackend::Shutdown();
}

void Process() {
    if (!Core::IsWorldReady()) return;
    Detail::WeaponBackend::Process();
}

bool GiveAll() { return Detail::WeaponBackend::GiveAll(); }
bool ClearAll() { return Detail::WeaponBackend::ClearAll(); }
bool DropWeapon() { return Detail::WeaponBackend::DropWeapon(); }
bool DropCurrent() { return Detail::WeaponBackend::DropCurrent(); }
int RemoveTrackedPickups() { return Detail::WeaponBackend::RemoveTrackedPickups(); }
bool Give(unsigned int weaponType, unsigned int ammo) { return Detail::WeaponBackend::Give(weaponType, ammo); }
bool GiveModel(unsigned int weaponModel, unsigned int ammo) { return Detail::WeaponBackend::GiveModel(weaponModel, ammo); }
bool MaxWeaponSkills() { return Detail::WeaponBackend::MaxWeaponSkills(); }
bool SetInfiniteAmmo(bool enable) { Detail::WeaponBackend::SetInfiniteAmmo(enable); return true; }
bool SetFastReload(bool enable) { Detail::WeaponBackend::SetFastReload(enable); return true; }
bool ResetStats() { return Detail::WeaponBackend::ResetStats(); }
void SetStatOverrides(const StatOverrides& overrides) { Detail::WeaponBackend::SetStatOverrides(overrides); }

} // namespace XBase::Weapon