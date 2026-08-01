#include <XBase/VehicleEffects.h>

#include "plugin.h"
#include "CModelInfo.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CShadows.h"
#include "CTimer.h"
#include "CVehicle.h"

#include <algorithm>

namespace {

struct NeonData {
    CRGBA color{255, 0, 0, 255};
    bool installed = false;
    bool pulsing = false;
    bool increment = true;
    float pulseOffset = 0.0f;

    explicit NeonData(CVehicle*) {}
};

plugin::VehicleExtendedData<NeonData> s_neonData;
bool s_renderSubscribed = false;

CVehicle* GetCurrentVehicle() {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !player->m_pVehicle) return nullptr;
    CVehicle* vehicle = player->m_pVehicle;
    return CPools::GetVehicleRef(vehicle) >= 0 ? vehicle : nullptr;
}

void RenderVehicleNeon(CVehicle* vehicle) {
    if (!vehicle || vehicle->IsUpsideDown()) return;
    NeonData& data = s_neonData.Get(vehicle);
    if (!data.installed || !gpShadowCarTex) return;

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(vehicle->m_nModelIndex);
    if (!modelInfo || !modelInfo->m_pColModel) return;

    const CVector bounds = modelInfo->m_pColModel->m_boundBox.m_vecMin;
    const CVector center = vehicle->TransformFromObjectSpace(CVector(0.0f, 0.0f, 0.0f));
    const CVector front = vehicle->TransformFromObjectSpace(
        CVector(0.0f, -bounds.y - data.pulseOffset, 0.0f)) - center;
    const CVector side = vehicle->TransformFromObjectSpace(
        CVector(bounds.x + data.pulseOffset, 0.0f, 0.0f)) - center;

    CShadows::StoreShadowToBeRendered(
        SHADOW_ADDITIVE,
        gpShadowCarTex,
        &center,
        front.x,
        front.y,
        side.x,
        side.y,
        180,
        data.color.r,
        data.color.g,
        data.color.b,
        2.0f,
        false,
        1.0f,
        nullptr,
        true);

    if (!data.pulsing) return;
    const unsigned int delta = CTimer::m_snTimeInMilliseconds - CTimer::m_snPreviousTimeInMilliseconds;
    data.pulseOffset += (data.increment ? 1.0f : -1.0f) * 0.0003f * static_cast<float>(delta);
    if (data.pulseOffset <= 0.0f) {
        data.pulseOffset = 0.0f;
        data.increment = true;
    } else if (data.pulseOffset >= 0.3f) {
        data.pulseOffset = 0.3f;
        data.increment = false;
    }
}

void RemoveCurrentNeon() {
    if (CVehicle* vehicle = GetCurrentVehicle()) {
        s_neonData.Get(vehicle).installed = false;
    }
}

} // namespace

namespace XBase::VehicleEffects {

bool ApplyCurrentNeon(const NeonSettings& settings) {
    CVehicle* vehicle = GetCurrentVehicle();
    if (!vehicle) return false;

    NeonData& data = s_neonData.Get(vehicle);
    data.installed = settings.enabled;
    data.pulsing = settings.pulsing;
    data.color = CRGBA(
        static_cast<unsigned char>(std::clamp(settings.red, 0, 255)),
        static_cast<unsigned char>(std::clamp(settings.green, 0, 255)),
        static_cast<unsigned char>(std::clamp(settings.blue, 0, 255)),
        255);
    return true;
}

void Init() {
    if (s_renderSubscribed) return;
    plugin::Events::vehicleRenderEvent += RenderVehicleNeon;
    s_renderSubscribed = true;
}

void NotifyGameInit() {
    RemoveCurrentNeon();
}

void Process() {
}

void Shutdown() {
    RemoveCurrentNeon();
    if (s_renderSubscribed) {
        plugin::Events::vehicleRenderEvent -= RenderVehicleNeon;
        s_renderSubscribed = false;
    }
}

} // namespace XBase::VehicleEffects