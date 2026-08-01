#include <XBase/Types.h>
#include "TypesSdk.h"
#include "plugin.h"
#include "CPed.h"

namespace XBase::Types {

bool IsMissionPed(CPed* ped) {
    if (!ped) return false;
    return ped->m_nCreatedBy == 2;
}

bool IsCopPed(const CPed* ped) {
    if (!ped) return false;
    return ped->m_nPedType == PED_TYPE_COP;
}

bool IsGangPed(const CPed* ped) {
    if (!ped) return false;
    const int t = ped->m_nPedType;
    return t >= 7 && t <= 15;
}

void ClearPedAiming(CPed* ped) {
    if (ped) ped->ClearLookFlag();
}

} // namespace XBase::Types
