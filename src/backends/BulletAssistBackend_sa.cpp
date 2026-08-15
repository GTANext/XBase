#include "BulletAssistBackend.h"
#include "common.h"

#include <XBase/Core.h>
#include <XBase/Ped.h>
#include "CCamera.h"
#include "CColModel.h"
#include "CColPoint.h"
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
    return {output.x, output.y, output.z};
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
    if (!s_config.tracking) return;
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
    if (!callback.active || s_fireDepth <= 0 || cameraIgnore) {
        return s_originalProcessLineOfSight(origin, target, point, entity, buildings, vehicles,
            peds, objects, dummies, seeThrough, cameraIgnore, shootThrough);
    }

    const CVector redirected = s_config.tracking && s_hasShotTarget
        ? ExtendPast(origin, s_shotTarget) : target;
    if (s_config.tracking && s_hasShotTarget) {
        peds = true;
    }
    if (s_config.throughWalls) {
        buildings = false;
        objects = false;
        dummies = false;
    }
    return s_originalProcessLineOfSight(origin, redirected, point, entity, buildings, vehicles,
        peds, objects, dummies, seeThrough, cameraIgnore, shootThrough);
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
                PedId{static_cast<std::uint32_t>(reference) + 1u}, Ped::GetNoFire())) return false;
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
                PedId{static_cast<std::uint32_t>(reference) + 1u}, Ped::GetNoFire())) return false;
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

void DrawBoneLine(ImDrawList* drawList, CPed* ped, ePedBones from, ePedBones to, ImU32 color) {
    const CVector fromPosition = BonePosition(ped, from);
    const CVector toPosition = BonePosition(ped, to);
    if (!BoneValid(fromPosition) || !BoneValid(toPosition)) return;
    DrawLine(drawList, fromPosition, toPosition, color);
}

void DrawSkeleton(ImDrawList* drawList, CPed* ped, ImU32 color) {
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_SPINE1, color);
    DrawBoneLine(drawList, ped, BONE_SPINE1, BONE_NECK, color);
    DrawBoneLine(drawList, ped, BONE_NECK, BONE_HEAD, color);
    DrawBoneLine(drawList, ped, BONE_NECK, BONE_LEFTSHOULDER, color);
    DrawBoneLine(drawList, ped, BONE_LEFTSHOULDER, BONE_LEFTELBOW, color);
    DrawBoneLine(drawList, ped, BONE_LEFTELBOW, BONE_LEFTHAND, color);
    DrawBoneLine(drawList, ped, BONE_NECK, BONE_RIGHTSHOULDER, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTSHOULDER, BONE_RIGHTELBOW, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTELBOW, BONE_RIGHTHAND, color);
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_LEFTHIP, color);
    DrawBoneLine(drawList, ped, BONE_LEFTHIP, BONE_LEFTKNEE, color);
    DrawBoneLine(drawList, ped, BONE_LEFTKNEE, BONE_LEFTFOOT, color);
    DrawBoneLine(drawList, ped, BONE_PELVIS, BONE_RIGHTHIP, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTHIP, BONE_RIGHTKNEE, color);
    DrawBoneLine(drawList, ped, BONE_RIGHTKNEE, BONE_RIGHTFOOT, color);
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
    if (s_config.tracking) CollectCandidates();
    else s_candidates.clear();
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
    s_config = {};
}

void Draw(const BulletAssist::Config& config) {
    if (!Core::IsWorldReady()) return;
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
            if (config.drawPedBounds || config.drawPedCollision) DrawEntityBounds(drawList, ped, IM_COL32(80, 220, 120, 230));
            if (config.drawPedSkeleton) DrawSkeleton(drawList, ped, IM_COL32(255, 200, 60, 230));
        }
    }
    if (CPools::ms_pVehiclePool && (config.drawVehicleBounds || config.drawVehicleCollision)) {
        for (int index = 0; index < CPools::ms_pVehiclePool->m_nSize; ++index) {
            CVehicle* vehicle = CPools::ms_pVehiclePool->GetAt(index);
            if (vehicle && vehicle->m_fHealth > 0.0f) DrawEntityBounds(drawList, vehicle, IM_COL32(255, 140, 60, 230));
        }
    }
}

bool ShouldSuppressPedFire(PedId ped, bool noFireEnabled) {
    return static_cast<bool>(ped) && noFireEnabled;
}

} // namespace XBase::Detail::BulletAssistBackend