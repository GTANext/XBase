#include <XBase/BulletAssist.h>
#include <XBase/Core.h>
#include <XBase/Ped.h>
#include <XBase/Types.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CPed.h"
#include "CPools.h"
#include "CWorld.h"
#include "CCamera.h"
#include "CWeapon.h"
#include "CRGBA.h"
#include "CSprite2d.h"
#include "RenderWare.h"

namespace XBase::BulletAssist {

static bool s_enabled = false;
static bool s_aimAtNearest = false;
static CPed* s_target = nullptr;

void Init() {
    s_enabled = true;
    s_aimAtNearest = true;
}

void Process() {
    if (!Core::IsWorldReady() || !s_enabled) {
        s_target = nullptr;
        return;
    }

    CPlayerPed* player = FindPlayerPed();
    if (!player) { s_target = nullptr; return; }

    s_target = player->m_pPlayerTargettedPed;

    if (s_target && CPools::GetPedRef(s_target) == -1) {
        s_target = nullptr;
    }
    if (!s_aimAtNearest) return;
    if (s_target) return;

    CPed* nearest = nullptr;
    float nearestDist = 50.0f;
    CVector ppos = player->GetPosition();
    for (CPed* p : CPools::ms_pPedPool) {
        if (!p || p == player || p->m_fHealth <= 0.0f) continue;
        CVector d = p->GetPosition() - ppos;
        float dist = d.Magnitude();
        if (dist < nearestDist) {
            nearest = p;
            nearestDist = dist;
        }
    }
    s_target = nearest;
}

void Draw() {
    if (!s_target || !Core::IsWorldReady()) return;

    float cx = static_cast<float>(RsGlobal.maximumWidth) * 0.5f;
    float cy = static_cast<float>(RsGlobal.maximumHeight) * 0.5f;

    CRGBA red(255, 50, 50, 180);
    CSprite2d::DrawRect(CRect(cx - 10.0f, cy - 1.0f, cx + 10.0f, cy + 1.0f), red);
    CSprite2d::DrawRect(CRect(cx - 1.0f, cy - 10.0f, cx + 1.0f, cy + 10.0f), red);
}

bool ShouldSuppressPedFire(CPed* ped) {
    return ped != nullptr && XBase::Ped::GetNoFire();
}

} // namespace XBase::BulletAssist
