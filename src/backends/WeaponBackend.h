#pragma once

#include <XBase/Weapon.h>

namespace XBase::Detail::WeaponBackend {

void Init();
void Shutdown();
void Process();
bool Give(unsigned int weaponType, unsigned int ammo);
bool GiveModel(unsigned int weaponModel, unsigned int ammo);
bool GiveAll();
bool ClearAll();
bool DropWeapon();
bool DropCurrent();
bool MaxWeaponSkills();
bool ResetStats();
void SetInfiniteAmmo(bool enable);
void SetFastReload(bool enable);
void SetStatOverrides(const Weapon::StatOverrides& overrides);
int RemoveTrackedPickups();

} // namespace XBase::Detail::WeaponBackend