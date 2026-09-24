#include "BulletAssistBackend.h"

#include <XBase/Core.h>
#include <XBase/Ped.h>

#include "common.h"
#include "CColModel.h"
#include "CEntity.h"
#include "CPed.h"
#include "CPools.h"
#include "CSprite.h"
#include "CVehicle.h"
#include "CVector.h"
#include "RenderWare.h"

#include "imgui.h"

namespace XBase::Detail::BulletAssistBackend {
namespace {

bool WorldToScreen(const CVector& world, ImVec2& screen) {
    RwV3d input{world.x, world.y, world.z};
    RwV3d output{};
    float width = 0.0f;
    float height = 0.0f;
    if (!CSprite::CalcScreenCoors(input, &output, &width, &height, true)) return false;
    screen = {output.x, output.y};
    return true;
}

void DrawLine(ImDrawList* drawList, const CVector& from, const CVector& to, ImU32 color) {
    ImVec2 screenFrom{};
    ImVec2 screenTo{};
    if (!WorldToScreen(from, screenFrom) || !WorldToScreen(to, screenTo)) return;
    drawList->AddLine(screenFrom, screenTo, color);
}

void DrawBounds(ImDrawList* drawList, CEntity* entity, ImU32 color) {
    CColModel* collision = entity ? entity->GetColModel() : nullptr;
    if (!collision) return;

    const CVector& minimum = collision->m_boundBox.m_vecMin;
    const CVector& maximum = collision->m_boundBox.m_vecMax;
    CVector corners[8] = {
        {minimum.x, minimum.y, minimum.z},
        {maximum.x, minimum.y, minimum.z},
        {maximum.x, maximum.y, minimum.z},
        {minimum.x, maximum.y, minimum.z},
        {minimum.x, minimum.y, maximum.z},
        {maximum.x, minimum.y, maximum.z},
        {maximum.x, maximum.y, maximum.z},
        {minimum.x, maximum.y, maximum.z},
    };
    for (CVector& corner : corners) corner = entity->TransformFromObjectSpace(corner);

    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    for (const auto& edge : edges) {
        DrawLine(drawList, corners[edge[0]], corners[edge[1]], color);
    }
}

bool IsValidPed(CPed* ped, CPed* player) {
    return ped && ped != player && ped->m_fHealth > 0.0f && ped->m_ePedState != PEDSTATE_DEAD;
}

} // namespace

bool Init() {
    // 只做只读的边界框显示。追踪与开火抑制要挂钩开火和视线函数，
    // 这个版本没有可用的挂钩地址，所以那部分保持未实现
    return true;
}

void Process(const BulletAssist::Config&) {
}

void Shutdown() {
}

void Draw(const BulletAssist::Config& config) {
    if (!config.drawPedBounds && !config.drawVehicleBounds) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    CPed* player = FindPlayerPed();

    if (config.drawPedBounds && CPools::ms_pPedPool) {
        for (int index = 0; index < CPools::ms_pPedPool->m_nSize; ++index) {
            CPed* ped = CPools::ms_pPedPool->GetAt(index);
            if (!IsValidPed(ped, player)) continue;
            DrawBounds(drawList, ped, IM_COL32(80, 220, 120, 230));
        }
    }

    if (config.drawVehicleBounds && CPools::ms_pVehiclePool) {
        for (int index = 0; index < CPools::ms_pVehiclePool->m_nSize; ++index) {
            CVehicle* vehicle = CPools::ms_pVehiclePool->GetAt(index);
            if (!vehicle || vehicle->m_fHealth <= 0.0f) continue;
            DrawBounds(drawList, vehicle, IM_COL32(255, 140, 60, 230));
        }
    }
}

bool ShouldSuppressPedFire(PedId, const Ped::NoFireOptions&) {
    // 抑制开火依赖挂钩开火函数，这个版本还没有挂钩点，因此一律不抑制
    return false;
}

} // namespace XBase::Detail::BulletAssistBackend
