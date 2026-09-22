#include "BulletAssistBackend.h"
#include "RuntimeGuard.h"
#include "common.h"
#include "../controllers/TypesSdk.h"

#include <XBase/Core.h>
#include <XBase/Ped.h>
#include "CCamera.h"
#include "CColBox.h"
#include "CColModel.h"
#include "CColPoint.h"
#include "CColSphere.h"
#include "CCollisionData.h"
#include "CEntity.h"
#include "CModelInfo.h"
#include "CPad.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CSprite.h"
#include "CTimer.h"
#include "CVector.h"
#include "CVehicle.h"
#include "CWeapon.h"
#include "ePedBones.h"
#include "ePedState.h"
#include "ePedType.h"
#include "eVehicleType.h"
#include "imgui.h"
#include "kiero/minhook/MinHook.h"
#include "RenderWare.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace XBase::Detail::BulletAssistBackend {
namespace {
using ProcessLineOfSightFn = bool(__cdecl*)(
    const CVector&, const CVector&, CColPoint&, CEntity*&,
    bool, bool, bool, bool, bool, bool, bool, bool);
using FireInstantHitFn = bool(__thiscall*)(
    CWeapon*, CEntity*, CVector*, CVector*, CEntity*, CVector*, CVector*, bool, bool);
using FireInstantHitFromCarFn = bool(__thiscall*)(CWeapon*, CVehicle*, bool, bool);

constexpr std::uintptr_t kFireInstantHitAddress = 0x73FB10;
constexpr std::uintptr_t kFireInstantHitFromCarAddress = 0x73EC40;
constexpr std::uintptr_t kProcessLineOfSightAddress = 0x56BA00;
constexpr float kPi = 3.14159265f;
constexpr float kMaxPitchUp = 1.05f;
constexpr float kMaxPitchDown = 1.49f;

ProcessLineOfSightFn s_originalProcessLineOfSight = nullptr;
FireInstantHitFn s_originalFireInstantHit = nullptr;
FireInstantHitFromCarFn s_originalFireInstantHitFromCar = nullptr;
BulletAssist::Config s_config;
std::atomic<unsigned int> s_inFlight{0};
std::atomic<bool> s_stopping{false};
bool s_ownsFireInstantHit = false;
bool s_ownsFireInstantHitFromCar = false;
bool s_ownsProcessLineOfSight = false;
int s_fireDepth = 0;
unsigned int s_roundRobin = 0;

struct Candidate {
    CPed* ped = nullptr;
    CVehicle* vehicle = nullptr;
    CVector position{};
    float score = 0.0f;
};

std::vector<Candidate> s_candidates;
CVector s_shotTarget{};
bool s_hasShotTarget = false;
CPed* s_hardLockPed = nullptr;

struct CallbackScope {
    bool active = false;
    CallbackScope() {
        if (s_stopping.load(std::memory_order_acquire)) return;
        s_inFlight.fetch_add(1, std::memory_order_acq_rel);
        if (s_stopping.load(std::memory_order_acquire)) {
            s_inFlight.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
        active = true;
    }
    ~CallbackScope() {
        if (active) s_inFlight.fetch_sub(1, std::memory_order_acq_rel);
    }
};

bool IsValidPed(CPed* ped, CPed* player) {
    return ped && ped != player && ped->m_fHealth > 0.0f
        && ped->m_ePedState != PEDSTATE_DEAD
        && ped->m_ePedState != PEDSTATE_DIE
        && ped->m_ePedState != PEDSTATE_DIE_BY_STEALTH;
}

bool IsMissionPed(CPed* ped) {
    return ped && ped->m_nCreatedBy == 2;
}

enum class Relation { Civilian, Friend, Hostile, Neutral };

bool AcquaintanceContains(unsigned int mask, int pedType) {
    return pedType >= 0 && pedType < 32 && (mask & (1u << static_cast<unsigned int>(pedType))) != 0;
}

Relation Classify(CPed* ped, CPed* player) {
    if (!IsMissionPed(ped)) return Relation::Civilian;
    const int playerType = player->m_nPedType;
    const int pedType = ped->m_nPedType;
    const CPedAcquaintance& pedAcq = ped->m_acquaintance;
    const CPedAcquaintance& playerAcq = player->m_acquaintance;
    if (AcquaintanceContains(pedAcq.m_nHate | pedAcq.m_nDislike, playerType)
        || AcquaintanceContains(playerAcq.m_nHate | playerAcq.m_nDislike, pedType)
        || pedType == PED_TYPE_COP) return Relation::Hostile;
    if (AcquaintanceContains(pedAcq.m_nLike | pedAcq.m_nRespect, playerType)
        || AcquaintanceContains(playerAcq.m_nLike | playerAcq.m_nRespect, pedType)) return Relation::Friend;
    return Relation::Neutral;
}

bool IsRelationEnabled(Relation relation) {
    switch (relation) {
    case Relation::Civilian: return s_config.trackCivilian;
    case Relation::Friend: return s_config.trackFriend;
    case Relation::Hostile: return s_config.trackHostile;
    case Relation::Neutral: return s_config.trackNeutral;
    }
    return false;
}

CVector BonePosition(CPed* ped, ePedBones bone) {
    RwV3d output{};
    ped->GetBonePosition(output, static_cast<unsigned int>(bone), false);
    // GetBonePosition returns object space so move it into world space with the ped matrix
    return ped->TransformFromObjectSpace(CVector(output.x, output.y, output.z));
}

bool BoneValid(const CVector& position) {
    return std::fabs(position.x) > 0.001f || std::fabs(position.y) > 0.001f || std::fabs(position.z) > 0.001f;
}

CVector PedAimPosition(CPed* ped) {
    CVector position = ped->GetPosition();
    switch (s_config.aimPart) {
    case BulletAssist::AimPart::Head:
        position = BonePosition(ped, BONE_HEAD);
        position.z -= 0.18f;
        break;
    case BulletAssist::AimPart::Abdomen:
        position = BonePosition(ped, BONE_PELVIS);
        position.z += 0.12f;
        break;
    case BulletAssist::AimPart::Legs: {
        const CVector left = BonePosition(ped, BONE_LEFTKNEE);
        const CVector right = BonePosition(ped, BONE_RIGHTKNEE);
        position = (left + right) * 0.5f;
        break;
    }
    case BulletAssist::AimPart::Chest:
    default:
        position = BonePosition(ped, BONE_SPINE1);
        position.z -= 0.05f;
        break;
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        position = ped->GetPosition();
        position.z += 0.55f;
    }
    return position;
}

bool IsHelicopter(CVehicle* vehicle) {
    return vehicle && (CModelInfo::IsHeliModel(vehicle->m_nModelIndex)
        || vehicle->m_nVehicleClass == VEHICLE_HELI
        || vehicle->m_nVehicleClass == VEHICLE_FHELI);
}

CVector VehicleAimPosition(CVehicle* vehicle) {
    CVector position = vehicle->GetPosition();
    position.z += 0.6f;
    return position;
}

bool CameraDirection(CVector& origin, CVector& direction) {
    const int index = TheCamera.m_nActiveCam;
    if (index < 0 || index > 2) return false;
    const CCam& camera = TheCamera.m_aCams[index];
    origin = camera.m_vecSource;
    direction = camera.m_vecFront;
    const float length = direction.Magnitude();
    if (length < 0.0001f || !std::isfinite(length)) return false;
    direction *= 1.0f / length;
    return true;
}

float CandidateScore(const CVector& cameraOrigin, const CVector& cameraDirection,
                     const CVector& playerPosition, const CVector& target) {
    CVector cameraDelta = target - cameraOrigin;
    const float cameraDistance = cameraDelta.Magnitude();
    if (cameraDistance < 0.05f) return -1.0f;
    cameraDelta *= 1.0f / cameraDistance;
    const float affinity = cameraDelta.x * cameraDirection.x
        + cameraDelta.y * cameraDirection.y
        + cameraDelta.z * cameraDirection.z;
    if (affinity < 0.25f) return -1.0f;
    const float distance = (target - playerPosition).Magnitude();
    if (distance > s_config.lockRange) return -1.0f;
    return affinity * 3.0f - distance / s_config.lockRange * 0.35f;
}

void CollectCandidates() {
    s_candidates.clear();
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    CVector cameraOrigin{};
    CVector cameraDirection{};
    if (!CameraDirection(cameraOrigin, cameraDirection)) return;
    const CVector playerPosition = player->GetPosition();

    if (CPools::ms_pPedPool) {
        for (int index = 0; index < CPools::ms_pPedPool->m_nSize; ++index) {
            CPed* ped = CPools::ms_pPedPool->GetAt(index);
            if (!IsValidPed(ped, player) || !IsRelationEnabled(Classify(ped, player))) continue;
            if (ped->m_pRwObject == nullptr) continue;
            if (ped->bInVehicle && ped->m_pVehicle && IsHelicopter(ped->m_pVehicle)) continue;
            const CVector target = PedAimPosition(ped);
            const float score = CandidateScore(cameraOrigin, cameraDirection, playerPosition, target);
            if (score >= 0.0f) s_candidates.push_back({ped, nullptr, target, score});
        }
    }
    if (CPools::ms_pVehiclePool) {
        for (int index = 0; index < CPools::ms_pVehiclePool->m_nSize; ++index) {
            CVehicle* vehicle = CPools::ms_pVehiclePool->GetAt(index);
            if (!vehicle || vehicle->m_fHealth <= 0.0f || !IsHelicopter(vehicle)
                || vehicle == player->m_pVehicle) continue;
            const Relation relation = vehicle->m_pDriver && vehicle->m_pDriver != player
                ? Classify(vehicle->m_pDriver, player) : Relation::Neutral;
            if (!IsRelationEnabled(relation)) continue;
            const CVector target = VehicleAimPosition(vehicle);
            const float score = CandidateScore(cameraOrigin, cameraDirection, playerPosition, target);
            if (score >= 0.0f) s_candidates.push_back({nullptr, vehicle, target, score});
        }
    }

    std::sort(s_candidates.begin(), s_candidates.end(), [](const Candidate& left, const Candidate& right) {
        return left.score > right.score;
    });
    if (static_cast<int>(s_candidates.size()) > s_config.maxTargets) {
        s_candidates.resize(static_cast<std::size_t>(s_config.maxTargets));
    }
}

void BeginShot() {
    ++s_fireDepth;
    if (s_fireDepth != 1) return;
    s_hasShotTarget = false;
    if (!s_config.tracking || !RuntimeGuard::IsRuntimeSafe()) return;
    CollectCandidates();
    if (s_candidates.empty()) return;
    const std::size_t index = s_candidates.size() == 1
        ? 0u : static_cast<std::size_t>(s_roundRobin++ % s_candidates.size());
    s_shotTarget = s_candidates[index].position;
    s_hasShotTarget = true;
}

void EndShot() {
    if (s_fireDepth <= 0) return;
    --s_fireDepth;
    if (s_fireDepth == 0) s_hasShotTarget = false;
}

struct ShotScope {
    bool active = false;
    explicit ShotScope(bool enabled) : active(enabled) { if (active) BeginShot(); }
    ~ShotScope() { if (active) EndShot(); }
};

bool IsLocalPlayer(CEntity* entity) {
    CPlayerPed* player = FindPlayerPed();
    return player && entity == static_cast<CEntity*>(player);
}

CVector ExtendPast(const CVector& origin, const CVector& target) {
    CVector delta = target - origin;
    const float length = delta.Magnitude();
    if (length < 0.05f) return target;
    return origin + delta * ((length + 0.45f) / length);
}

bool __cdecl HookProcessLineOfSight(
    const CVector& origin, const CVector& target, CColPoint& point, CEntity*& entity,
    bool buildings, bool vehicles, bool peds, bool objects, bool dummies,
    bool seeThrough, bool cameraIgnore, bool shootThrough) {
    CallbackScope callback;
    if (!s_originalProcessLineOfSight) return false;
    if (!callback.active || s_fireDepth <= 0 || cameraIgnore || !RuntimeGuard::IsRuntimeSafe()) {
        return s_originalProcessLineOfSight(origin, target, point, entity, buildings, vehicles,
            peds, objects, dummies, seeThrough, cameraIgnore, shootThrough);
    }

    const bool tracked = s_config.tracking && s_hasShotTarget;
    const CVector redirected = tracked ? ExtendPast(origin, s_shotTarget) : target;
    if (tracked) {
        peds = true;
    }
    if (s_config.throughWalls) {
        buildings = false;
        objects = false;
        dummies = false;
    }
    const bool hit = s_originalProcessLineOfSight(origin, redirected, point, entity, buildings, vehicles,
        peds, objects, dummies, seeThrough, cameraIgnore, shootThrough);
    if (hit && (tracked || s_config.throughWalls) && entity
        && entity->m_nType == ENTITY_TYPE_VEHICLE) {
        CVehicle* vehicle = static_cast<CVehicle*>(entity);
        if (vehicle->m_fHealth <= 0.0f) {
            entity = nullptr;
            return false;
        }
    }
    return hit;
}

bool __fastcall HookFireInstantHit(
    CWeapon* weapon, void*, CEntity* firingEntity, CVector* origin, CVector* muzzle,
    CEntity* targetEntity, CVector* target, CVector* driveByOrigin, bool arg6, bool drawMuzzle) {
    CallbackScope callback;
    if (!s_originalFireInstantHit) return false;
    if (!callback.active) {
        return s_originalFireInstantHit(weapon, firingEntity, origin, muzzle, targetEntity,
            target, driveByOrigin, arg6, drawMuzzle);
    }
    if (firingEntity && firingEntity->m_nType == ENTITY_TYPE_PED && !IsLocalPlayer(firingEntity)) {
        CPed* ped = static_cast<CPed*>(firingEntity);
        const int reference = CPools::GetPedRef(ped);
        if (reference >= 0 && ShouldSuppressPedFire(
                PedId{static_cast<std::uint32_t>(reference) + 1u}, Ped::GetNoFireOptions())) return false;
    }

    ShotScope shot(IsLocalPlayer(firingEntity));
    CVector saved{};
    const bool redirect = shot.active && s_config.tracking && s_hasShotTarget && target;
    if (redirect) {
        saved = *target;
        *target = s_shotTarget;
    }
    const bool result = s_originalFireInstantHit(weapon, firingEntity, origin, muzzle,
        targetEntity, target, driveByOrigin, arg6, drawMuzzle);
    if (redirect) *target = saved;
    return result;
}

bool __fastcall HookFireInstantHitFromCar(
    CWeapon* weapon, void*, CVehicle* vehicle, bool left, bool right) {
    CallbackScope callback;
    if (!s_originalFireInstantHitFromCar) return false;
    CPlayerPed* player = FindPlayerPed();
    if (!callback.active) return s_originalFireInstantHitFromCar(weapon, vehicle, left, right);
    if (vehicle && vehicle->m_pDriver && vehicle->m_pDriver != player) {
        const int reference = CPools::GetPedRef(vehicle->m_pDriver);
        if (reference >= 0 && ShouldSuppressPedFire(
                PedId{static_cast<std::uint32_t>(reference) + 1u}, Ped::GetNoFireOptions())) return false;
    }
    ShotScope shot(player && player->m_pVehicle == vehicle);
    return s_originalFireInstantHitFromCar(weapon, vehicle, left, right);
}

bool InstallHook(std::uintptr_t address, void* detour, void** original, bool& owned) {
    void* target = reinterpret_cast<void*>(address);
    if (MH_CreateHook(target, detour, original) != MH_OK) return false;
    const MH_STATUS enabled = MH_EnableHook(target);
    owned = enabled == MH_OK || enabled == MH_ERROR_ENABLED;
    if (!owned) MH_RemoveHook(target);
    return owned;
}

void RemoveHook(std::uintptr_t address, bool& owned) {
    if (!owned) return;
    void* target = reinterpret_cast<void*>(address);
    MH_DisableHook(target);
    MH_RemoveHook(target);
    owned = false;
}

bool WorldToScreen(const CVector& world, ImVec2& screen) {
    RwV3d input{world.x, world.y, world.z};
    RwV3d output{};
    float width = 0.0f;
    float height = 0.0f;
    if (!CSprite::CalcScreenCoors(input, &output, &width, &height, true, true)
        || width < 1.0f || height < 1.0f) {
        return false;
    }
    screen = {output.x, output.y};
    return true;
}

float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

float LerpAngle(float from, float to, float factor) {
    return from + NormalizeAngle(to - from) * factor;
}

void SetCamFrontFromAngles(CCam& camera) {
    const float cosVertical = std::cos(camera.m_fVerticalAngle);
    const float sinVertical = std::sin(camera.m_fVerticalAngle);
    const float cosHorizontal = std::cos(camera.m_fHorizontalAngle);
    const float sinHorizontal = std::sin(camera.m_fHorizontalAngle);
    camera.m_vecFront = CVector(
        -cosHorizontal * cosVertical, -sinHorizontal * cosVertical, sinVertical);
    camera.m_fAlphaSpeed = 0.0f;
    camera.m_fBetaSpeed = 0.0f;
}

void ApplyHardLockAim(const CVector& worldTarget) {
    const int index = TheCamera.m_nActiveCam;
    if (index < 0 || index > 2) return;
    CCam& camera = TheCamera.m_aCams[index];

    float blend = 0.55f;
    if (CTimer::ms_fTimeStep > 0.0f) {
        blend = std::min(1.0f, 0.35f * CTimer::ms_fTimeStep);
    }
    if (blend < 0.22f) blend = 0.22f;

    ImVec2 screen{};
    if (WorldToScreen(worldTarget, screen)) {
        const float halfWidth = std::max(1.0f, static_cast<float>(RsGlobal.maximumWidth) * 0.5f);
        const float halfHeight = std::max(1.0f, static_cast<float>(RsGlobal.maximumHeight) * 0.5f);
        const float errorX = screen.x - halfWidth;
        const float errorY = screen.y - halfHeight;

        float fovDegrees = camera.m_fFOV;
        if (fovDegrees < 5.0f || fovDegrees > 170.0f || !std::isfinite(fovDegrees)) {
            fovDegrees = 70.0f;
        }
        const float halfFovY = fovDegrees * 0.5f * (kPi / 180.0f);
        const float aspect = halfWidth / halfHeight;
        const float halfFovX = std::atan(std::tan(halfFovY) * aspect);

        const float horizontal = camera.m_fHorizontalAngle - (errorX / halfWidth) * halfFovX;
        const float vertical = std::clamp(
            camera.m_fVerticalAngle - (errorY / halfHeight) * halfFovY,
            -kMaxPitchDown, kMaxPitchUp);

        camera.m_fHorizontalAngle = LerpAngle(camera.m_fHorizontalAngle, horizontal, blend);
        camera.m_fVerticalAngle = LerpAngle(camera.m_fVerticalAngle, vertical, blend);
        SetCamFrontFromAngles(camera);
        return;
    }

    const float deltaX = worldTarget.x - camera.m_vecSource.x;
    const float deltaY = worldTarget.y - camera.m_vecSource.y;
    const float deltaZ = worldTarget.z - camera.m_vecSource.z;
    const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    if (length < 0.05f || !std::isfinite(length)) return;

    const float inverse = 1.0f / length;
    const float vertical = std::clamp(
        std::asin(std::clamp(deltaZ * inverse, -1.0f, 1.0f)), -kMaxPitchDown, kMaxPitchUp);
    const float horizontal = std::atan2(-deltaY * inverse, -deltaX * inverse);

    camera.m_fHorizontalAngle = LerpAngle(camera.m_fHorizontalAngle, horizontal, blend);
    camera.m_fVerticalAngle = LerpAngle(camera.m_fVerticalAngle, vertical, blend);
    SetCamFrontFromAngles(camera);
}

void ClearHardLock() {
    s_hardLockPed = nullptr;
}

CPed* ResolveHardLockPed() {
    if (s_candidates.empty()) {
        ClearHardLock();
        return nullptr;
    }
    if (s_hardLockPed) {
        for (const Candidate& candidate : s_candidates) {
            if (candidate.ped == s_hardLockPed) return s_hardLockPed;
        }
    }
    for (const Candidate& candidate : s_candidates) {
        if (candidate.ped) {
            s_hardLockPed = candidate.ped;
            return s_hardLockPed;
        }
    }
    ClearHardLock();
    return nullptr;
}

bool PlayerWantsHardLockInput() {
    CPad* pad = CPad::GetPad(0);
    if (!pad) return false;
    if (pad->GetTarget()) return true;
    if (pad->NewState.ButtonCircle != 0) return true;
    if (CPad::NewMouseControllerState.lmb) return true;
    return false;
}

void ApplyHardLock(const BulletAssist::Config& config) {
    if (!config.hardLock || !config.tracking || !PlayerWantsHardLockInput()) {
        if (!config.hardLock || !config.tracking) ClearHardLock();
        return;
    }
    CPed* ped = ResolveHardLockPed();
    if (!ped) return;
    for (const Candidate& candidate : s_candidates) {
        if (candidate.ped == ped) {
            ApplyHardLockAim(candidate.position);
            return;
        }
    }
    ApplyHardLockAim(PedAimPosition(ped));
}

void DrawLine(ImDrawList* drawList, const CVector& from, const CVector& to, ImU32 color) {
    ImVec2 screenFrom{};
    ImVec2 screenTo{};
    if (WorldToScreen(from, screenFrom) && WorldToScreen(to, screenTo)) {
        drawList->AddLine(screenFrom, screenTo, color, 1.4f);
    }
}

void DrawEntityBounds(ImDrawList* drawList, CEntity* entity, ImU32 color) {
    CColModel* collision = entity ? entity->GetColModel() : nullptr;
    if (!collision) return;
    const CVector& minimum = collision->m_boundBox.m_vecMin;
    const CVector& maximum = collision->m_boundBox.m_vecMax;
    CVector corners[8] = {
        {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
        {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
        {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
        {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z},
    };
    for (CVector& corner : corners) corner = entity->TransformFromObjectSpace(corner);
    constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (const auto& edge : edges) DrawLine(drawList, corners[edge[0]], corners[edge[1]], color);
}

void DrawLocalBoxWire(ImDrawList* drawList, CEntity* entity, const CVector& minimum, const CVector& maximum, ImU32 color) {
    if (!entity) return;
    CVector corners[8] = {
        entity->TransformFromObjectSpace(CVector(minimum.x, minimum.y, minimum.z)),
        entity->TransformFromObjectSpace(CVector(maximum.x, minimum.y, minimum.z)),
        entity->TransformFromObjectSpace(CVector(maximum.x, maximum.y, minimum.z)),
        entity->TransformFromObjectSpace(CVector(minimum.x, maximum.y, minimum.z)),
        entity->TransformFromObjectSpace(CVector(minimum.x, minimum.y, maximum.z)),
        entity->TransformFromObjectSpace(CVector(maximum.x, minimum.y, maximum.z)),
        entity->TransformFromObjectSpace(CVector(maximum.x, maximum.y, maximum.z)),
        entity->TransformFromObjectSpace(CVector(minimum.x, maximum.y, maximum.z)),
    };
    constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (const auto& edge : edges) DrawLine(drawList, corners[edge[0]], corners[edge[1]], color);
}

void DrawLocalSphereWire(ImDrawList* drawList, CEntity* entity, const CVector& center, float radius, ImU32 color) {
    if (!entity || radius < 0.01f) return;
    constexpr int segments = 12;
    CVector previousXY{};
    CVector previousXZ{};
    CVector previousYZ{};
    for (int index = 0; index <= segments; ++index) {
        const float angle = kPi * 2.0f * static_cast<float>(index) / static_cast<float>(segments);
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        const CVector pointXY = entity->TransformFromObjectSpace(
            CVector(center.x + radius * cosAngle, center.y + radius * sinAngle, center.z));
        const CVector pointXZ = entity->TransformFromObjectSpace(
            CVector(center.x + radius * cosAngle, center.y, center.z + radius * sinAngle));
        const CVector pointYZ = entity->TransformFromObjectSpace(
            CVector(center.x, center.y + radius * cosAngle, center.z + radius * sinAngle));
        if (index > 0) {
            DrawLine(drawList, previousXY, pointXY, color);
            DrawLine(drawList, previousXZ, pointXZ, color);
            DrawLine(drawList, previousYZ, pointYZ, color);
        }
        previousXY = pointXY;
        previousXZ = pointXZ;
        previousYZ = pointYZ;
    }
}

void DrawCollision(ImDrawList* drawList, CEntity* entity, ImU32 boxColor, ImU32 sphereColor) {
    CColModel* collision = entity ? entity->GetColModel() : nullptr;
    if (!collision || !collision->m_pColData) return;
    CCollisionData* data = collision->m_pColData;
    if (data->m_pBoxes) {
        for (unsigned short index = 0; index < data->m_nNumBoxes; ++index) {
            DrawLocalBoxWire(drawList, entity, data->m_pBoxes[index].m_vecMin, data->m_pBoxes[index].m_vecMax, boxColor);
        }
    }
    if (data->m_pSpheres) {
        for (unsigned short index = 0; index < data->m_nNumSpheres; ++index) {
            DrawLocalSphereWire(drawList, entity, data->m_pSpheres[index].m_vecCenter, data->m_pSpheres[index].m_fRadius, sphereColor);
        }
    }
}

void DrawBoneLine(ImDrawList* drawList, CPed* ped, ePedBones from, ePedBones to, ImU32 color) {
    const CVector fromPosition = BonePosition(ped, from);
    const CVector toPosition = BonePosition(ped, to);
    if (!BoneValid(fromPosition) || !BoneValid(toPosition)) return;
    DrawLine(drawList, fromPosition, toPosition, color);
}

void DrawSkeleton(ImDrawList* drawList, CPed* ped, ImU32 color) {
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_SPINE1, color);
    DrawBoneLine(drawList, ped, BONE_SPINE1, BONE_UPPERTORSO, color);
    DrawBoneLine(drawList, ped, BONE_UPPERTORSO, BONE_NECK, color);
    DrawBoneLine(drawList, ped, BONE_NECK, BONE_HEAD, color);
    DrawBoneLine(drawList, ped, BONE_UPPERTORSO, BONE_LEFTSHOULDER, color);
    DrawBoneLine(drawList, ped, BONE_LEFTSHOULDER, BONE_LEFTELBOW, color);
    DrawBoneLine(drawList, ped, BONE_LEFTELBOW, BONE_LEFTWRIST, color);
    DrawBoneLine(drawList, ped, BONE_LEFTWRIST, BONE_LEFTHAND, color);
    DrawBoneLine(drawList, ped, BONE_UPPERTORSO, BONE_RIGHTSHOULDER, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTSHOULDER, BONE_RIGHTELBOW, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTELBOW, BONE_RIGHTWRIST, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTWRIST, BONE_RIGHTHAND, color);
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_LEFTHIP, color);
    DrawBoneLine(drawList, ped, BONE_LEFTHIP, BONE_LEFTKNEE, color);
    DrawBoneLine(drawList, ped, BONE_LEFTKNEE, BONE_LEFTANKLE, color);
    DrawBoneLine(drawList, ped, BONE_LEFTANKLE, BONE_LEFTFOOT, color);
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_RIGHTHIP, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTHIP, BONE_RIGHTKNEE, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTKNEE, BONE_RIGHTANKLE, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTANKLE, BONE_RIGHTFOOT, color);
}
}

bool Init() {
    s_stopping.store(false, std::memory_order_release);
    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;

    const bool fire = InstallHook(kFireInstantHitAddress, reinterpret_cast<void*>(&HookFireInstantHit),
        reinterpret_cast<void**>(&s_originalFireInstantHit), s_ownsFireInstantHit);
    const bool car = InstallHook(kFireInstantHitFromCarAddress, reinterpret_cast<void*>(&HookFireInstantHitFromCar),
        reinterpret_cast<void**>(&s_originalFireInstantHitFromCar), s_ownsFireInstantHitFromCar);
    const bool line = InstallHook(kProcessLineOfSightAddress, reinterpret_cast<void*>(&HookProcessLineOfSight),
        reinterpret_cast<void**>(&s_originalProcessLineOfSight), s_ownsProcessLineOfSight);
    if (fire && car && line) return true;
    Shutdown();
    return false;
}

void Process(const BulletAssist::Config& config) {
    s_config = config;
    if (!RuntimeGuard::IsRuntimeSafe()) {
        s_candidates.clear();
        s_hasShotTarget = false;
        ClearHardLock();
        return;
    }
    if (s_config.tracking) CollectCandidates();
    else s_candidates.clear();
    ApplyHardLock(s_config);
}

void Shutdown() {
    s_stopping.store(true, std::memory_order_release);
    RemoveHook(kProcessLineOfSightAddress, s_ownsProcessLineOfSight);
    RemoveHook(kFireInstantHitFromCarAddress, s_ownsFireInstantHitFromCar);
    RemoveHook(kFireInstantHitAddress, s_ownsFireInstantHit);
    while (s_inFlight.load(std::memory_order_acquire) != 0) std::this_thread::yield();
    s_originalProcessLineOfSight = nullptr;
    s_originalFireInstantHit = nullptr;
    s_originalFireInstantHitFromCar = nullptr;
    s_candidates.clear();
    s_fireDepth = 0;
    s_hasShotTarget = false;
    s_hardLockPed = nullptr;
    s_config = {};
}

void Draw(const BulletAssist::Config& config) {
    if (!Core::IsWorldReady() || !RuntimeGuard::IsRuntimeSafe()) return;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    if (config.tracking) {
        const CVector playerPosition = player->GetPosition();
        for (const Candidate& candidate : s_candidates) {
            ImVec2 screen{};
            if (WorldToScreen(candidate.position, screen)) drawList->AddCircle(screen, 12.0f, IM_COL32(255, 40, 40, 255), 16, 2.0f);
            DrawLine(drawList, playerPosition, candidate.position, IM_COL32(255, 40, 40, 220));
        }
    }
    if (CPools::ms_pPedPool && (config.drawPedBounds || config.drawPedCollision || config.drawPedSkeleton)) {
        for (int index = 0; index < CPools::ms_pPedPool->m_nSize; ++index) {
            CPed* ped = CPools::ms_pPedPool->GetAt(index);
            if (!IsValidPed(ped, player)) continue;
            if (config.drawPedBounds) DrawEntityBounds(drawList, ped, IM_COL32(80, 220, 120, 230));
            if (config.drawPedCollision) DrawCollision(drawList, ped, IM_COL32(60, 180, 255, 220), IM_COL32(120, 200, 255, 200));
            if (config.drawPedSkeleton) DrawSkeleton(drawList, ped, IM_COL32(255, 200, 60, 230));
        }
    }
    if (CPools::ms_pVehiclePool && (config.drawVehicleBounds || config.drawVehicleCollision)) {
        for (int index = 0; index < CPools::ms_pVehiclePool->m_nSize; ++index) {
            CVehicle* vehicle = CPools::ms_pVehiclePool->GetAt(index);
            if (!vehicle || vehicle->m_fHealth <= 0.0f) continue;
            if (config.drawVehicleBounds) DrawEntityBounds(drawList, vehicle, IM_COL32(255, 140, 60, 230));
            if (config.drawVehicleCollision) DrawCollision(drawList, vehicle, IM_COL32(255, 90, 90, 220), IM_COL32(255, 160, 120, 200));
        }
    }
}

namespace {

CPed* ResolvePed(PedId ped) {
    if (!ped || !CPools::ms_pPedPool) return nullptr;
    const int index = static_cast<int>(ped.value - 1u);
    if (index < 0 || index >= CPools::ms_pPedPool->m_nSize) return nullptr;
    CPed* candidate = CPools::ms_pPedPool->GetAt(index);
    return candidate && CPools::GetPedRef(candidate) == index ? candidate : nullptr;
}

} // namespace

bool ShouldSuppressPedFire(PedId ped, const Ped::NoFireOptions& options) {
    CPed* target = ResolvePed(ped);
    if (!target) return false;
    return ShouldSuppressNoFire(
        options, Types::IsMissionPed(target), Types::IsCopPed(target), Types::IsGangPed(target));
}

} // namespace XBase::Detail::BulletAssistBackend