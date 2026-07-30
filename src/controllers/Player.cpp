#include <XBase/Player.h>
#include <XBase/Core.h>
#include <XBase/Log.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CWorld.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CCheat.h"
#include "CTxdStore.h"
#include "CCamera.h"
#include "CPad.h"
#include "extensions/ScriptCommands.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <cstdint>

namespace {

bool IsPedInPool(CPed* ped) {
    if (!ped || !CPools::ms_pPedPool) return false;
    for (CPed* p : CPools::ms_pPedPool) {
        if (p == ped) return true;
    }
    return false;
}

float ClampHealth(float value) {
    if (value <= 0.0f) return 0.0f;
    return value < 2.0f ? 2.0f : value;
}

bool IsValidPedModel(unsigned int modelId) {
    const int model = static_cast<int>(modelId);
    return model >= 0 && model < CModelInfo::ms_modelInfoCount && CModelInfo::IsPedModel(model);
}

bool s_godMode = false;
bool s_invisible = false;
bool s_hardMode = false;
bool s_freeFly = false;
bool s_autoHeal = false;
bool s_respawnAtDeathPosition = false;
bool s_freezeWantedLevel = false;
int s_frozenWantedLevel = 0;
CPlayerPed* s_trackedPlayer = nullptr;
XBase::Types::ProofState s_savedProofs;
float s_savedHealth = 0.0f;
float s_savedArmour = 0.0f;
bool s_savedVisible = true;
bool s_hasSnapshot = false;
bool s_ownsProofs = false;
bool s_ownsVisibility = false;
bool s_ownsHealth = false;
bool s_ownsArmour = false;
float s_previousHealth = 0.0f;
float s_previousArmour = 0.0f;
std::chrono::steady_clock::time_point s_lastDamageTime{};
std::chrono::steady_clock::time_point s_lastHealTime{};
CVector s_deathPosition(0.0f, 0.0f, 0.0f);
bool s_hasDeathPosition = false;

struct BoolPatchState {
    bool applied = false;
    bool original = false;
};

struct FloatPatchState {
    bool applied = false;
    float original = 0.0f;
};

struct BytePatchState {
    bool applied = false;
    unsigned char original[2] = {};
};

BoolPatchState s_infiniteSprintPatch;
BoolPatchState s_superJumpPatch;
BoolPatchState s_superPunchPatch;
BoolPatchState s_underwaterBreathingPatch;
BoolPatchState s_cycleJumpPatch;
BoolPatchState s_neverHungryPatch;
BoolPatchState s_neverWantedPatch;
BoolPatchState s_freeFlyPatch;
FloatPatchState s_fastSprintPatch;
BytePatchState s_sprintEverywherePatch;
bool s_keepStuffApplied = false;
bool s_drunkEffectApplied = false;

void SetBoolPatch(std::uintptr_t address, bool enable, BoolPatchState& state) {
    bool* value = reinterpret_cast<bool*>(address);
    if (enable) {
        if (state.applied) return;
        state.original = *value;
        *value = true;
        state.applied = true;
        return;
    }
    if (!state.applied) return;
    *value = state.original;
    state.applied = false;
}

bool HasPersistentEffect() {
    return s_godMode || s_invisible || s_hardMode || s_freeFly;
}

void DiscardSnapshot() {
    s_trackedPlayer = nullptr;
    s_hasSnapshot = false;
    s_ownsProofs = false;
    s_ownsVisibility = false;
    s_ownsHealth = false;
    s_ownsArmour = false;
}

void RestoreSnapshot() {
    if (s_hasSnapshot && IsPedInPool(s_trackedPlayer)) {
        if (s_ownsProofs) {
            s_trackedPlayer->bBulletProof = s_savedProofs.bullet;
            s_trackedPlayer->bCollisionProof = s_savedProofs.collision;
            s_trackedPlayer->bExplosionProof = s_savedProofs.explosion;
            s_trackedPlayer->bFireProof = s_savedProofs.fire;
            s_trackedPlayer->bMeleeProof = s_savedProofs.melee;
        }
        if (s_ownsVisibility) s_trackedPlayer->bIsVisible = s_savedVisible;
        if (s_ownsHealth) s_trackedPlayer->m_fHealth = s_savedHealth;
        if (s_ownsArmour) s_trackedPlayer->m_fArmour = s_savedArmour;
    }
    DiscardSnapshot();
}

void EnsureSnapshot(CPlayerPed* player) {
    if (!player) return;
    if (s_hasSnapshot && s_trackedPlayer == player) return;
    RestoreSnapshot();
    s_trackedPlayer = player;
    s_savedProofs.bullet = player->bBulletProof;
    s_savedProofs.collision = player->bCollisionProof;
    s_savedProofs.explosion = player->bExplosionProof;
    s_savedProofs.fire = player->bFireProof;
    s_savedProofs.melee = player->bMeleeProof;
    s_savedHealth = player->m_fHealth;
    s_savedArmour = player->m_fArmour;
    s_savedVisible = player->bIsVisible;
    s_previousHealth = s_savedHealth;
    s_previousArmour = s_savedArmour;
    s_hasSnapshot = true;
}

void ProcessAutoHeal(CPlayerPed* player) {
    const auto now = std::chrono::steady_clock::now();
    if (player->m_fHealth < s_previousHealth || player->m_fArmour < s_previousArmour) {
        s_lastDamageTime = now;
    }
    s_previousHealth = player->m_fHealth;
    s_previousArmour = player->m_fArmour;
    if (!s_autoHeal || now - s_lastDamageTime < std::chrono::seconds(5)
        || now - s_lastHealTime < std::chrono::seconds(1)) return;

    const float maxHealth = s_hardMode ? 50.0f : 100.0f;
    if (player->m_fHealth < maxHealth) {
        player->m_fHealth = std::min(maxHealth, player->m_fHealth + 2.0f);
        s_previousHealth = player->m_fHealth;
    } else if (!s_hardMode && player->m_fArmour > 0.0f && player->m_fArmour < 100.0f) {
        player->m_fArmour = std::min(100.0f, player->m_fArmour + 2.0f);
        s_previousArmour = player->m_fArmour;
    }
    s_lastHealTime = now;
}

void ProcessRespawn(CPlayerPed* player) {
    if (!s_respawnAtDeathPosition) {
        s_hasDeathPosition = false;
        return;
    }
    if (player->m_fHealth <= 0.0f) {
        s_deathPosition = player->GetPosition();
        s_hasDeathPosition = true;
        return;
    }
    if (!s_hasDeathPosition) return;
    const CVector current = player->GetPosition();
    if (current.x != s_deathPosition.x || current.y != s_deathPosition.y) {
        player->Teleport(s_deathPosition, false);
        s_hasDeathPosition = false;
    }
}

} // namespace

namespace XBase::Player {

CPlayerPed* Get() {
    return FindPlayerPed();
}

void* GetHandle() {
    return Get();
}

void Process() {
    if (!Core::IsWorldReady()) return;
    CPlayerPed* player = Get();
    if (!player || !IsPedInPool(player)) {
        RestoreSnapshot();
        return;
    }
    if (HasPersistentEffect()) {
        EnsureSnapshot(player);

        if (!s_godMode && !s_freeFly && s_ownsProofs) {
            player->bBulletProof = s_savedProofs.bullet;
            player->bCollisionProof = s_savedProofs.collision;
            player->bExplosionProof = s_savedProofs.explosion;
            player->bFireProof = s_savedProofs.fire;
            player->bMeleeProof = s_savedProofs.melee;
            s_ownsProofs = false;
        }
        if (!s_invisible && s_ownsVisibility) {
            player->bIsVisible = s_savedVisible;
            s_ownsVisibility = false;
        }
        if (!s_godMode && !s_hardMode && s_ownsHealth) {
            player->m_fHealth = s_savedHealth;
            s_ownsHealth = false;
        }
        if (!s_hardMode && s_ownsArmour) {
            player->m_fArmour = s_savedArmour;
            s_ownsArmour = false;
        }

        if (s_godMode || s_freeFly) {
            player->bBulletProof = s_godMode || s_savedProofs.bullet;
            player->bFireProof = s_godMode || s_savedProofs.fire;
            player->bExplosionProof = s_godMode || s_savedProofs.explosion;
            player->bCollisionProof = s_godMode || s_freeFly || s_savedProofs.collision;
            player->bMeleeProof = s_godMode || s_savedProofs.melee;
            s_ownsProofs = true;
        }
        if (s_invisible) {
            player->bIsVisible = false;
            s_ownsVisibility = true;
        }

        if (s_hardMode) {
            s_ownsHealth = true;
            s_ownsArmour = true;
            if (player->m_fHealth > 50.0f) {
                player->m_fHealth = 50.0f;
                s_ownsHealth = true;
            }
            if (player->m_fArmour != 0.0f) {
                player->m_fArmour = 0.0f;
                s_ownsArmour = true;
            }
        } else if (s_godMode) {
            if (player->m_fHealth < s_savedHealth) {
                player->m_fHealth = s_savedHealth;
                s_ownsHealth = true;
            }
            if (player->m_fArmour < s_savedArmour) {
                player->m_fArmour = s_savedArmour;
                s_ownsArmour = true;
            }
        }

        if (s_freeFly) {
            player->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
    } else {
        RestoreSnapshot();
    }
    if (s_freezeWantedLevel) SetWantedLevel(s_frozenWantedLevel);
    ProcessAutoHeal(player);
    ProcessRespawn(player);
}

void NotifyGameInit() {
    s_godMode = false;
    s_invisible = false;
    s_hardMode = false;
    s_freeFly = false;
    s_autoHeal = false;
    s_respawnAtDeathPosition = false;
    s_freezeWantedLevel = false;
    s_hasDeathPosition = false;

    // 游戏对象和作弊布尔状态由 initGame 重建；这里只恢复不会随存档重建的代码/常量补丁。
    SetFastSprint(false);
    SetSprintEverywhere(false);
    s_infiniteSprintPatch.applied = false;
    s_superJumpPatch.applied = false;
    s_superPunchPatch.applied = false;
    s_underwaterBreathingPatch.applied = false;
    s_cycleJumpPatch.applied = false;
    s_neverHungryPatch.applied = false;
    s_neverWantedPatch.applied = false;
    s_freeFlyPatch.applied = false;
    s_keepStuffApplied = false;
    s_drunkEffectApplied = false;
    DiscardSnapshot();
}

void Shutdown() {
    s_godMode = false;
    s_invisible = false;
    s_hardMode = false;
    s_autoHeal = false;
    s_respawnAtDeathPosition = false;
    s_freezeWantedLevel = false;
    s_hasDeathPosition = false;
    SetInfiniteSprint(false);
    SetKeepStuff(false);
    SuperJump(false);
    SuperPunch(false);
    UnderwaterBreathing(false);
    SetCycleJump(false);
    SetNeverHungry(false);
    SetFastSprint(false);
    SetSprintEverywhere(false);
    SetDrunkEffect(false);
    SetNeverWanted(false);
    SetFreeFly(false);
    RestoreSnapshot();
}

void SetRuntimeOptions(const RuntimeOptions& options) {
    s_godMode = options.godMode;
    s_invisible = options.invisible;
    s_hardMode = options.hardMode;
    s_autoHeal = options.autoHeal;
    s_respawnAtDeathPosition = options.respawnAtDeathPosition;
    s_freezeWantedLevel = options.freezeWantedLevel;
    s_frozenWantedLevel = std::clamp(options.wantedLevel, 0, 6);
    SetFreeFly(options.freeFlyProtection);
    if (!HasPersistentEffect()) RestoreSnapshot();
}

RuntimeOptions GetRuntimeOptions() {
    RuntimeOptions options;
    options.godMode = s_godMode;
    options.invisible = s_invisible;
    options.hardMode = s_hardMode;
    options.autoHeal = s_autoHeal;
    options.respawnAtDeathPosition = s_respawnAtDeathPosition;
    options.freezeWantedLevel = s_freezeWantedLevel;
    options.wantedLevel = s_frozenWantedLevel;
    options.freeFlyProtection = s_freeFly;
    return options;
}

bool MoveRelative(float forward, float right, float up) {
    CPlayerPed* player = Get();
    if (!player) return false;
    const float angle = player->m_fHeadingCurrent;
    CVector position = player->GetPosition();
    position.x += -std::sin(angle) * forward + std::cos(angle) * right;
    position.y += std::cos(angle) * forward + std::sin(angle) * right;
    position.z += up;
    player->Teleport(position, false);
    return true;
}

void Heal() {
    CPlayerPed* player = Get();
    if (player) player->m_fHealth = 100.0f;
}

void GiveArmour() {
    CPlayerPed* player = Get();
    if (player) player->m_fArmour = 100.0f;
}

void GiveMoney(int amount) {
    CWorld::Players[CWorld::PlayerInFocus].m_nMoney += amount;
}

void Kill() {
    CPlayerPed* player = Get();
    if (player) player->m_fHealth = 0.0f;
}

int GetWantedLevel() {
    CPlayerPed* player = Get();
    return player ? player->GetWantedLevel() : 0;
}

void SetWantedLevel(int level) {
    CPlayerPed* player = Get();
    if (!player) return;
    if (level < 0) level = 0;
    if (level > 6) level = 6;
    player->SetWantedLevel(level);
}

void ClearWantedLevel() {
    SetWantedLevel(0);
}

int GetMoney() {
    return CWorld::Players[CWorld::PlayerInFocus].m_nMoney;
}

void SetMoney(int amount) {
    CWorld::Players[CWorld::PlayerInFocus].m_nMoney = amount;
}

float GetHealth() {
    CPlayerPed* player = Get();
    return player ? player->m_fHealth : 0.0f;
}

void SetHealth(float value) {
    CPlayerPed* player = Get();
    if (!player) return;
    const float health = ClampHealth(value);
    player->m_fHealth = health;
    if (s_hasSnapshot && s_trackedPlayer == player) {
        s_savedHealth = health;
        s_previousHealth = health;
        s_ownsHealth = false;
    }
}

float GetArmour() {
    CPlayerPed* player = Get();
    return player ? player->m_fArmour : 0.0f;
}

void SetArmour(float value) {
    CPlayerPed* player = Get();
    if (!player) return;
    player->m_fArmour = value;
    if (s_hasSnapshot && s_trackedPlayer == player) {
        s_savedArmour = value;
        s_previousArmour = value;
        s_ownsArmour = false;
    }
}

Types::ProofState GetProofState() {
    Types::ProofState state;
    CPlayerPed* player = Get();
    if (!player) return state;
    state.bullet = player->bBulletProof;
    state.collision = player->bCollisionProof;
    state.explosion = player->bExplosionProof;
    state.fire = player->bFireProof;
    state.melee = player->bMeleeProof;
    return state;
}

void SetProofState(const Types::ProofState& state) {
    CPlayerPed* player = Get();
    if (!player) return;
    if (s_hasSnapshot && s_trackedPlayer == player) {
        s_savedProofs = state;
        if (s_godMode || s_freeFly) {
            player->bBulletProof = s_godMode || state.bullet;
            player->bCollisionProof = s_godMode || s_freeFly || state.collision;
            player->bExplosionProof = s_godMode || state.explosion;
            player->bFireProof = s_godMode || state.fire;
            player->bMeleeProof = s_godMode || state.melee;
            s_ownsProofs = true;
            return;
        }
        s_ownsProofs = false;
    }
    player->bBulletProof = state.bullet;
    player->bCollisionProof = state.collision;
    player->bExplosionProof = state.explosion;
    player->bFireProof = state.fire;
    player->bMeleeProof = state.melee;
}

bool SetSkin(unsigned int modelId) {
    if (!IsValidPedModel(modelId)) return false;
    CPlayerPed* player = Get();
    if (!player) return false;
    const int model = static_cast<int>(modelId);
    CStreaming::RequestModel(model, PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(false);
    plugin::Command<plugin::Commands::SET_PLAYER_MODEL>(0, model);
    plugin::Command<plugin::Commands::BUILD_PLAYER_MODEL>(0);
    plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    return true;
}

bool ApplyClothes(int textureId, int modelId, int bodyPart) {
    plugin::Command<plugin::Commands::GIVE_PLAYER_CLOTHES>(0, textureId, modelId, bodyPart);
    plugin::Command<plugin::Commands::BUILD_PLAYER_MODEL>(0);
    return true;
}

void SetInfiniteSprint(bool enable) {
    SetBoolPatch(0xB7CEE4, enable, s_infiniteSprintPatch);
}

void SetKeepStuff(bool enable) {
    if (s_keepStuffApplied == enable) return;
    plugin::Command<plugin::Commands::SWITCH_ARREST_PENALTIES>(enable);
    plugin::Command<plugin::Commands::SWITCH_DEATH_PENALTIES>(enable);
    s_keepStuffApplied = enable;
}

void SetFreeHealthcare(bool enable) {
    CWorld::Players[CWorld::PlayerInFocus].m_bGetOutOfHospitalFree = enable;
}

bool GetFreeHealthcare() {
    return CWorld::Players[CWorld::PlayerInFocus].m_bGetOutOfHospitalFree;
}

void SetFreeJail(bool enable) {
    CWorld::Players[CWorld::PlayerInFocus].m_bGetOutOfJailFree = enable;
}

bool GetFreeJail() {
    return CWorld::Players[CWorld::PlayerInFocus].m_bGetOutOfJailFree;
}

void MaxVehicleSkills() {
    CCheat::VehicleSkillsCheat();
}

bool SetStat(int statId, float value) {
    plugin::Command<plugin::Commands::SET_FLOAT_STAT>(statId, value);
    return true;
}

void SuperJump(bool enable) {
    SetBoolPatch(0x96916C, enable, s_superJumpPatch);
}

void SuperPunch(bool enable) {
    SetBoolPatch(0x969173, enable, s_superPunchPatch);
}

void UnderwaterBreathing(bool enable) {
    SetBoolPatch(0x96916E, enable, s_underwaterBreathingPatch);
}

void SetCycleJump(bool enable) {
    SetBoolPatch(0x969161, enable, s_cycleJumpPatch);
}

void SetNeverHungry(bool enable) {
    SetBoolPatch(0x969174, enable, s_neverHungryPatch);
}

void SetFastSprint(bool enable) {
    float* value = reinterpret_cast<float*>(0x8D2458);
    if (enable) {
        if (s_fastSprintPatch.applied) return;
        s_fastSprintPatch.original = *value;
        *value = 0.1f;
        s_fastSprintPatch.applied = true;
        return;
    }
    if (!s_fastSprintPatch.applied) return;
    *value = s_fastSprintPatch.original;
    s_fastSprintPatch.applied = false;
}

void SetSprintEverywhere(bool enable) {
    unsigned char* value = reinterpret_cast<unsigned char*>(0x688610);
    if (enable) {
        if (s_sprintEverywherePatch.applied) return;
        std::memcpy(s_sprintEverywherePatch.original, value, sizeof(s_sprintEverywherePatch.original));
        value[0] = 0x90;
        value[1] = 0x90;
        s_sprintEverywherePatch.applied = true;
        return;
    }
    if (!s_sprintEverywherePatch.applied) return;
    std::memcpy(value, s_sprintEverywherePatch.original, sizeof(s_sprintEverywherePatch.original));
    s_sprintEverywherePatch.applied = false;
}

void SetDrunkEffect(bool enable) {
    if (s_drunkEffectApplied == enable) return;
    plugin::Command<plugin::Commands::SET_PLAYER_DRUNKENNESS>(0, enable ? 100 : 0);
    s_drunkEffectApplied = enable;
}

void SetNeverWanted(bool enable) {
    SetBoolPatch(0x969171, enable, s_neverWantedPatch);
    if (enable) ClearWantedLevel();
}

void SetGodMode(bool enable) {
    s_godMode = enable;
    if (!HasPersistentEffect()) RestoreSnapshot();
}

bool IsGodMode() {
    return s_godMode;
}

void SetInvisible(bool enable) {
    s_invisible = enable;
    if (!HasPersistentEffect()) RestoreSnapshot();
}

bool IsInvisible() {
    return s_invisible;
}

void SetHardMode(bool enable) {
    s_hardMode = enable;
    if (!HasPersistentEffect()) RestoreSnapshot();
}

bool IsHardMode() {
    return s_hardMode;
}

void SetFreeFly(bool enable) {
    s_freeFly = enable;
    SetBoolPatch(0x969175, enable, s_freeFlyPatch);
    if (enable) {
        CPlayerPed* player = Get();
        if (player) {
            player->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
    }
}

bool IsFreeFly() {
    return s_freeFly;
}

void CopyCoordinates() {
    CPlayerPed* player = Get();
    if (!player) return;
    CVector pos = player->GetPosition();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "X: %.2f Y: %.2f Z: %.2f", pos.x, pos.y, pos.z);
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, std::strlen(buffer) + 1);
    if (hMem) {
        char* pMem = static_cast<char*>(GlobalLock(hMem));
        if (pMem) {
            std::strcpy(pMem, buffer);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    CloseClipboard();
}

bool RequestSaveGame() {
    *reinterpret_cast<bool*>(0x96918C) = true;
    return true;
}

void MoveForward(float distance) {
    MoveRelative(distance, 0.0f, 0.0f);
}

void MoveUp(float distance) {
    MoveRelative(0.0f, 0.0f, distance);
}

void MoveDown(float distance) {
    MoveRelative(0.0f, 0.0f, -distance);
}

bool SetCustomSkin(const char* txdName) {
    if (!txdName || !txdName[0]) return false;
    CPlayerPed* player = Get();
    if (!player) return false;

    int slot = CTxdStore::FindTxdSlot(txdName);
    if (slot == -1) {
        slot = CTxdStore::AddTxdSlot(txdName);
    }
    char path[MAX_PATH];
    std::snprintf(path, sizeof(path), "models\\txd\\%s.txd", txdName);

    if (slot >= 0 && CTxdStore::LoadTxd(slot, path)) {
        CTxdStore::AddRef(slot);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(slot);

        bool loaded = plugin::Command<plugin::Commands::SET_PLAYER_MODEL>(0, 0);
        loaded &= plugin::Command<plugin::Commands::BUILD_PLAYER_MODEL>(0);

        CTxdStore::PopCurrentTxd();
        return loaded;
    }
    CTxdStore::RemoveTxdSlot(slot);
    return false;
}

void ApplyAimSkinChanger() {
    CPlayerPed* player = Get();
    if (!player || !Core::IsWorldReady()) return;

    CPad* pad = CPad::GetPad(0);
    bool aiming = (pad->NewState.RightShoulder1 > 0 || pad->NewState.ButtonCircle > 0);
    if (!aiming) return;

    static unsigned int s_skinCycle = 0;
    static const unsigned int skinCycleModels[] = {
        9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38
    };
    const int count = sizeof(skinCycleModels) / sizeof(skinCycleModels[0]);
    s_skinCycle = (s_skinCycle + 1) % count;
    SetSkin(skinCycleModels[s_skinCycle]);
}

} // namespace XBase::Player
