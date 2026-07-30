#pragma once

namespace XBase::Weapon {

void Process();
void GiveAll();
void ClearAll();
void DropWeapon();
void DropCurrent();
int RemoveTrackedPickups();
void Give(unsigned int weaponType, unsigned int ammo);
void GiveModel(unsigned int weaponModel, unsigned int ammo);
void MaxWeaponSkills();
void SetInfiniteAmmo(bool enable);
void SetFastReload(bool enable);
void ResetStats();

} // namespace XBase::Weapon
