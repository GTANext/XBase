#pragma once

class CPed;

namespace XBase::Types {

bool IsMissionPed(CPed* ped);
bool IsCopPed(const CPed* ped);
bool IsGangPed(const CPed* ped);
void ClearPedAiming(CPed* ped);

} // namespace XBase::Types