#include "PlayerBackend.h"

#include "CPlayerPed.h"
#include "CPlayerInfo.h"
#include "CWorld.h"
#include "common.h"
#include <cmath>

namespace XBase::Detail::PlayerBackend {

namespace {
CPlayerInfo* GetPlayerInfo() {
    return CWorld::Players ? &CWorld::Players[CWorld::PlayerInFocus] : nullptr;
}

CPlayerPed* AsPlayer(void* player) {
    return static_cast<CPlayerPed*>(player);
}
} // namespace

void* GetPlayer() { return FindPlayerPed(); }
bool IsPlayerValid(void* player) { return player && FindPlayerPed() == player; }
float GetHealth(void* player) { return player ? AsPlayer(player)->m_fHealth : 0.0f; }
void SetHealth(void* player, float value) { if (player) AsPlayer(player)->m_fHealth = value; }
float GetArmour(void* player) { return player ? AsPlayer(player)->m_fArmour : 0.0f; }
void SetArmour(void* player, float value) { if (player) AsPlayer(player)->m_fArmour = value; }
int GetWantedLevel(void* player) { return player ? AsPlayer(player)->GetWantedLevel() : 0; }
void SetWantedLevel(void* player, int level) { if (player) AsPlayer(player)->SetWantedLevel(level); }
int GetMoney() { const auto* info = GetPlayerInfo(); return info ? info->m_nMoney : 0; }
void SetMoney(int amount) { auto* info = GetPlayerInfo(); if (info) info->m_nMoney = amount; }

Types::ProofState GetProofState(void* player) {
    Types::ProofState state;
    const auto* ped = AsPlayer(player);
    if (!ped) return state;
    state.bullet = ped->bBulletProof;
    state.collision = ped->bCollisionProof;
    state.explosion = ped->bExplosionProof;
    state.fire = ped->bFireProof;
    state.melee = ped->bMeleeProof;
    state.nonPlayer = ped->bImmuneToNonPlayerDamage;
    return state;
}

void SetProofState(void* player, const Types::ProofState& state) {
    auto* ped = AsPlayer(player);
    if (!ped) return;
    ped->bBulletProof = state.bullet;
    ped->bCollisionProof = state.collision;
    ped->bExplosionProof = state.explosion;
    ped->bFireProof = state.fire;
    ped->bMeleeProof = state.melee;
    ped->bImmuneToNonPlayerDamage = state.nonPlayer;
}

bool GetVisible(void* player) { return player && AsPlayer(player)->bIsVisible; }
void SetVisible(void* player, bool visible) { if (player) AsPlayer(player)->bIsVisible = visible; }

bool MoveRelative(void* player, float forward, float right, float up) {
    auto* ped = AsPlayer(player);
    if (!ped) return false;
    const float angle = ped->m_fHeadingCurrent;
    CVector position = ped->GetPosition();
    position.x += -std::sin(angle) * forward + std::cos(angle) * right;
    position.y += std::cos(angle) * forward + std::sin(angle) * right;
    position.z += up;
    ped->Teleport(position);
    return true;
}

Position GetPosition(void* player) {
    auto* ped = AsPlayer(player);
    if (!ped) return {};
    const CVector position = ped->GetPosition();
    return { position.x, position.y, position.z };
}

bool Teleport(void* player, const Position& position) {
    auto* ped = AsPlayer(player);
    if (!ped) return false;
    ped->Teleport(CVector(position.x, position.y, position.z));
    return true;
}

bool GetFreeHealthcare() { const auto* info = GetPlayerInfo(); return info && info->m_bGetOutOfHospitalFree; }
void SetFreeHealthcare(bool enable) { auto* info = GetPlayerInfo(); if (info) info->m_bGetOutOfHospitalFree = enable; }
bool GetFreeJail() { const auto* info = GetPlayerInfo(); return info && info->m_bGetOutOfJailFree; }
void SetFreeJail(bool enable) { auto* info = GetPlayerInfo(); if (info) info->m_bGetOutOfJailFree = enable; }
bool GetInfiniteSprint() { const auto* info = GetPlayerInfo(); return info && info->m_bInfiniteSprint; }
void SetInfiniteSprint(bool enable) { auto* info = GetPlayerInfo(); if (info) info->m_bInfiniteSprint = enable; }

} // namespace XBase::Detail::PlayerBackend