#include "RuntimeGuard.h"

#include <XBase/Core.h>

#include "common.h"
#include "CCamera.h"
#include "CCutsceneMgr.h"
#include "CPad.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CTimer.h"
#include "ePedState.h"

namespace XBase::Detail::RuntimeGuard {
namespace {

constexpr unsigned int kResumeDelayMs = 750;

unsigned int s_lastUnsafeTick = 0;

bool IsUnsafeWorldState() {
    if (!Core::IsWorldReady()) return true;
    if (CCutsceneMgr::ms_running || CCutsceneMgr::ms_cutsceneProcessing) return true;
    if (CCutsceneMgr::ms_numCutsceneObjs > 0) return true;
    if (TheCamera.m_bWideScreenOn || TheCamera.m_bStartingSpline) return true;
    if (!TheCamera.m_pRwCamera) return true;

    CPad* pad = CPad::GetPad(0);
    if (!pad || pad->DisablePlayerControls != 0) return true;

    if (!CPools::ms_pPedPool) return true;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return true;
    if (player->m_fHealth <= 0.0f) return true;
    if (player->m_ePedState == PEDSTATE_DIE
        || player->m_ePedState == PEDSTATE_DEAD
        || player->m_ePedState == PEDSTATE_DIE_BY_STEALTH) return true;
    return false;
}

} // namespace

bool Init() {
    s_lastUnsafeTick = 0;
    return true;
}

void Shutdown() {
    s_lastUnsafeTick = 0;
}

bool IsRuntimeSafe() {
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (IsUnsafeWorldState()) {
        s_lastUnsafeTick = now;
        return false;
    }
    if (s_lastUnsafeTick != 0 && now - s_lastUnsafeTick < kResumeDelayMs) return false;
    return true;
}

} // namespace XBase::Detail::RuntimeGuard
