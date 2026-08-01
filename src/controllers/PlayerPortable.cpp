#include <XBase/Player.h>
#include <XBase/Platform.h>
#include "PlayerBackend.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace {

using XBase::Detail::PlayerBackend::GetPlayer;
using XBase::Detail::PlayerBackend::IsPlayerValid;

struct PlayerState {
    bool godMode = false;
    bool invisible = false;
    bool hardMode = false;
    bool freeFly = false;
    bool autoHeal = false;
    bool respawnAtDeathPosition = false;
    bool freezeWantedLevel = false;
    int wantedLevel = 0;
    void* trackedPlayer = nullptr;
    XBase::Types::ProofState savedProofs;
    float savedHealth = 0.0f;
    float savedArmour = 0.0f;
    bool savedVisible = true;
    bool hasSnapshot = false;
    bool ownsProofs = false;
    bool ownsVisibility = false;
    bool ownsHealth = false;
    bool ownsArmour = false;
    float previousHealth = 0.0f;
    float previousArmour = 0.0f;
    std::chrono::steady_clock::time_point lastDamageTime{};
    std::chrono::steady_clock::time_point lastHealTime{};
    XBase::Detail::PlayerBackend::Position deathPosition;
    bool hasDeathPosition = false;
    bool infiniteSprintApplied = false;
    bool savedInfiniteSprint = false;
};

PlayerState s_state;

float ClampHealth(float value) {
    if (value <= 0.0f) return 0.0f;
    return value < 2.0f ? 2.0f : value;
}

int ClampWantedLevel(int level) {
    return std::clamp(level, 0, 6);
}

void DiscardSnapshot() {
    s_state.trackedPlayer = nullptr;
    s_state.hasSnapshot = false;
    s_state.ownsProofs = false;
    s_state.ownsVisibility = false;
    s_state.ownsHealth = false;
    s_state.ownsArmour = false;
}

void RestoreSnapshot() {
    if (s_state.hasSnapshot && IsPlayerValid(s_state.trackedPlayer)) {
        if (s_state.ownsProofs) {
            XBase::Detail::PlayerBackend::SetProofState(s_state.trackedPlayer, s_state.savedProofs);
        }
        if (s_state.ownsVisibility) {
            XBase::Detail::PlayerBackend::SetVisible(s_state.trackedPlayer, s_state.savedVisible);
        }
        if (s_state.ownsHealth) {
            XBase::Detail::PlayerBackend::SetHealth(s_state.trackedPlayer, s_state.savedHealth);
        }
        if (s_state.ownsArmour) {
            XBase::Detail::PlayerBackend::SetArmour(s_state.trackedPlayer, s_state.savedArmour);
        }
    }
    DiscardSnapshot();
}

void EnsureSnapshot(void* player) {
    if (!player) return;
    if (s_state.hasSnapshot && s_state.trackedPlayer == player) return;
    RestoreSnapshot();
    s_state.trackedPlayer = player;
    s_state.savedProofs = XBase::Detail::PlayerBackend::GetProofState(player);
    s_state.savedHealth = ClampHealth(XBase::Detail::PlayerBackend::GetHealth(player));
    s_state.savedArmour = XBase::Detail::PlayerBackend::GetArmour(player);
    s_state.savedVisible = XBase::Detail::PlayerBackend::GetVisible(player);
    s_state.previousHealth = s_state.savedHealth;
    s_state.previousArmour = s_state.savedArmour;
    s_state.hasSnapshot = true;
}

bool HasPersistentEffect() {
    return s_state.godMode || s_state.invisible || s_state.hardMode || s_state.freeFly;
}

void ProcessAutoHeal(void* player) {
    const auto now = std::chrono::steady_clock::now();
    float health = XBase::Detail::PlayerBackend::GetHealth(player);
    float armour = XBase::Detail::PlayerBackend::GetArmour(player);
    if (health < s_state.previousHealth || armour < s_state.previousArmour) {
        s_state.lastDamageTime = now;
    }
    s_state.previousHealth = health;
    s_state.previousArmour = armour;
    if (!s_state.autoHeal || now - s_state.lastDamageTime < std::chrono::seconds(5)
        || now - s_state.lastHealTime < std::chrono::seconds(1)) return;

    const float maxHealth = s_state.hardMode ? 50.0f : 100.0f;
    if (health < maxHealth) {
        health = std::min(maxHealth, health + 2.0f);
        XBase::Detail::PlayerBackend::SetHealth(player, health);
        s_state.previousHealth = health;
    } else if (!s_state.hardMode && armour > 0.0f && armour < 100.0f) {
        armour = std::min(100.0f, armour + 2.0f);
        XBase::Detail::PlayerBackend::SetArmour(player, armour);
        s_state.previousArmour = armour;
    }
    s_state.lastHealTime = now;
}

void ProcessRespawn(void* player) {
    if (!s_state.respawnAtDeathPosition) {
        s_state.hasDeathPosition = false;
        return;
    }
    if (XBase::Detail::PlayerBackend::GetHealth(player) <= 0.0f) {
        s_state.deathPosition = XBase::Detail::PlayerBackend::GetPosition(player);
        s_state.hasDeathPosition = true;
        return;
    }
    if (!s_state.hasDeathPosition) return;
    const auto current = XBase::Detail::PlayerBackend::GetPosition(player);
    if (current.x != s_state.deathPosition.x || current.y != s_state.deathPosition.y) {
        XBase::Detail::PlayerBackend::Teleport(player, s_state.deathPosition);
        s_state.hasDeathPosition = false;
    }
}

} // namespace

namespace XBase::Player {

bool IsAvailable() {
    return Detail::PlayerBackend::GetPlayer() != nullptr;
}

PlayerSnapshot GetSnapshot() {
    PlayerSnapshot snapshot;
    void* player = Detail::PlayerBackend::GetPlayer();
    if (!player || !Detail::PlayerBackend::IsPlayerValid(player)) return snapshot;

    const Detail::PlayerBackend::Position position = Detail::PlayerBackend::GetPosition(player);
    snapshot.valid = true;
    snapshot.position = {position.x, position.y, position.z};
    snapshot.health = Detail::PlayerBackend::GetHealth(player);
    snapshot.armour = Detail::PlayerBackend::GetArmour(player);
    snapshot.money = Detail::PlayerBackend::GetMoney();
    snapshot.wantedLevel = Detail::PlayerBackend::GetWantedLevel(player);
    snapshot.proofs = Detail::PlayerBackend::GetProofState(player);
    return snapshot;
}

void Process() {
    void* player = Detail::PlayerBackend::GetPlayer();
    if (!player || !Detail::PlayerBackend::IsPlayerValid(player)) {
        RestoreSnapshot();
        s_state.hasDeathPosition = false;
        return;
    }

    if (HasPersistentEffect()) {
        EnsureSnapshot(player);

        if (!s_state.godMode && !s_state.freeFly && s_state.ownsProofs) {
            Detail::PlayerBackend::SetProofState(player, s_state.savedProofs);
            s_state.ownsProofs = false;
        }
        if (!s_state.invisible && s_state.ownsVisibility) {
            Detail::PlayerBackend::SetVisible(player, s_state.savedVisible);
            s_state.ownsVisibility = false;
        }
        if (!s_state.godMode && !s_state.hardMode && s_state.ownsHealth) {
            Detail::PlayerBackend::SetHealth(player, s_state.savedHealth);
            s_state.ownsHealth = false;
        }
        if (!s_state.hardMode && s_state.ownsArmour) {
            Detail::PlayerBackend::SetArmour(player, s_state.savedArmour);
            s_state.ownsArmour = false;
        }

        Types::ProofState proofs = s_state.savedProofs;
        if (s_state.godMode) {
            proofs.bullet = true;
            proofs.collision = true;
            proofs.explosion = true;
            proofs.fire = true;
            proofs.melee = true;
            proofs.nonPlayer = true;
        } else if (s_state.freeFly) {
            proofs.collision = true;
        }
        if (s_state.godMode || s_state.freeFly) {
            Detail::PlayerBackend::SetProofState(player, proofs);
            s_state.ownsProofs = true;
        }
        if (s_state.invisible) {
            Detail::PlayerBackend::SetVisible(player, false);
            s_state.ownsVisibility = true;
        }

        if (s_state.hardMode) {
            s_state.ownsHealth = true;
            s_state.ownsArmour = true;
            if (Detail::PlayerBackend::GetHealth(player) > 50.0f) {
                Detail::PlayerBackend::SetHealth(player, 50.0f);
                s_state.ownsHealth = true;
            }
            if (Detail::PlayerBackend::GetArmour(player) != 0.0f) {
                Detail::PlayerBackend::SetArmour(player, 0.0f);
                s_state.ownsArmour = true;
            }
        } else if (s_state.godMode) {
            if (Detail::PlayerBackend::GetHealth(player) < s_state.savedHealth) {
                Detail::PlayerBackend::SetHealth(player, s_state.savedHealth);
                s_state.ownsHealth = true;
            }
            if (Detail::PlayerBackend::GetArmour(player) < s_state.savedArmour) {
                Detail::PlayerBackend::SetArmour(player, s_state.savedArmour);
                s_state.ownsArmour = true;
            }
        }
    } else {
        RestoreSnapshot();
    }

    if (s_state.freezeWantedLevel) {
        Detail::PlayerBackend::SetWantedLevel(player, ClampWantedLevel(s_state.wantedLevel));
    }
    ProcessAutoHeal(player);
    ProcessRespawn(player);
}

void NotifyGameInit() {
    s_state.godMode = false;
    s_state.invisible = false;
    s_state.hardMode = false;
    s_state.freeFly = false;
    s_state.autoHeal = false;
    s_state.respawnAtDeathPosition = false;
    s_state.freezeWantedLevel = false;
    s_state.hasDeathPosition = false;
    s_state.infiniteSprintApplied = false;
    DiscardSnapshot();
}

void Shutdown() {
    s_state.godMode = false;
    s_state.invisible = false;
    s_state.hardMode = false;
    s_state.freeFly = false;
    s_state.autoHeal = false;
    s_state.respawnAtDeathPosition = false;
    s_state.freezeWantedLevel = false;
    s_state.hasDeathPosition = false;
    SetInfiniteSprint(false);
    RestoreSnapshot();
}

void SetRuntimeOptions(const RuntimeOptions& options) {
    s_state.godMode = options.godMode;
    s_state.invisible = options.invisible;
    s_state.hardMode = options.hardMode;
    s_state.autoHeal = options.autoHeal;
    s_state.respawnAtDeathPosition = options.respawnAtDeathPosition;
    s_state.freezeWantedLevel = options.freezeWantedLevel;
    s_state.wantedLevel = ClampWantedLevel(options.wantedLevel);
    s_state.freeFly = options.freeFlyProtection;
    if (!HasPersistentEffect()) RestoreSnapshot();
}

RuntimeOptions GetRuntimeOptions() {
    RuntimeOptions options;
    options.godMode = s_state.godMode;
    options.invisible = s_state.invisible;
    options.hardMode = s_state.hardMode;
    options.autoHeal = s_state.autoHeal;
    options.respawnAtDeathPosition = s_state.respawnAtDeathPosition;
    options.freezeWantedLevel = s_state.freezeWantedLevel;
    options.wantedLevel = s_state.wantedLevel;
    options.freeFlyProtection = s_state.freeFly;
    return options;
}

bool MoveRelative(float forward, float right, float up) {
    return Detail::PlayerBackend::MoveRelative(Detail::PlayerBackend::GetPlayer(), forward, right, up);
}

void Heal() { SetHealth(100.0f); }
void GiveArmour() { SetArmour(100.0f); }
void GiveMoney(int amount) { SetMoney(GetMoney() + amount); }
void Kill() { Detail::PlayerBackend::SetHealth(Detail::PlayerBackend::GetPlayer(), 0.0f); }
int GetWantedLevel() { return ClampWantedLevel(Detail::PlayerBackend::GetWantedLevel(Detail::PlayerBackend::GetPlayer())); }
void SetWantedLevel(int level) { Detail::PlayerBackend::SetWantedLevel(Detail::PlayerBackend::GetPlayer(), ClampWantedLevel(level)); }
void ClearWantedLevel() { SetWantedLevel(0); }
int GetMoney() { return Detail::PlayerBackend::GetMoney(); }
void SetMoney(int amount) { Detail::PlayerBackend::SetMoney(amount); }
float GetHealth() { return Detail::PlayerBackend::GetHealth(Detail::PlayerBackend::GetPlayer()); }
void SetHealth(float value) {
    void* player = Detail::PlayerBackend::GetPlayer();
    const float health = ClampHealth(value);
    Detail::PlayerBackend::SetHealth(player, health);
    if (s_state.hasSnapshot && s_state.trackedPlayer == player) {
        s_state.savedHealth = health;
        s_state.previousHealth = health;
        s_state.ownsHealth = false;
    }
}
float GetArmour() { return Detail::PlayerBackend::GetArmour(Detail::PlayerBackend::GetPlayer()); }
void SetArmour(float value) {
    void* player = Detail::PlayerBackend::GetPlayer();
    const float armour = std::max(0.0f, value);
    Detail::PlayerBackend::SetArmour(player, armour);
    if (s_state.hasSnapshot && s_state.trackedPlayer == player) {
        s_state.savedArmour = armour;
        s_state.previousArmour = armour;
        s_state.ownsArmour = false;
    }
}
Types::ProofState GetProofState() { return Detail::PlayerBackend::GetProofState(Detail::PlayerBackend::GetPlayer()); }
void SetProofState(const Types::ProofState& state) {
    void* player = Detail::PlayerBackend::GetPlayer();
    if (s_state.hasSnapshot && s_state.trackedPlayer == player) {
        s_state.savedProofs = state;
        if (s_state.godMode || s_state.freeFly) {
            Types::ProofState effective = state;
            if (s_state.godMode) {
                effective.bullet = true;
                effective.collision = true;
                effective.explosion = true;
                effective.fire = true;
                effective.melee = true;
                effective.nonPlayer = true;
            } else {
                effective.collision = true;
            }
            Detail::PlayerBackend::SetProofState(player, effective);
            s_state.ownsProofs = true;
            return;
        }
        s_state.ownsProofs = false;
    }
    Detail::PlayerBackend::SetProofState(player, state);
}

bool SetSkin(unsigned int) { return false; }
bool SetCustomSkin(const char*) { return false; }
bool ApplyClothes(int, int, int) { return false; }
void SetInfiniteSprint(bool enable) {
    if (enable) {
        if (s_state.infiniteSprintApplied) return;
        s_state.savedInfiniteSprint = Detail::PlayerBackend::GetInfiniteSprint();
        Detail::PlayerBackend::SetInfiniteSprint(true);
        s_state.infiniteSprintApplied = true;
        return;
    }
    if (!s_state.infiniteSprintApplied) return;
    Detail::PlayerBackend::SetInfiniteSprint(s_state.savedInfiniteSprint);
    s_state.infiniteSprintApplied = false;
}
void SetKeepStuff(bool) {}
void SetFreeHealthcare(bool enable) { Detail::PlayerBackend::SetFreeHealthcare(enable); }
bool GetFreeHealthcare() { return Detail::PlayerBackend::GetFreeHealthcare(); }
void SetFreeJail(bool enable) { Detail::PlayerBackend::SetFreeJail(enable); }
bool GetFreeJail() { return Detail::PlayerBackend::GetFreeJail(); }
void MaxVehicleSkills() {}
bool SetStat(int, float) { return false; }

void SuperJump(bool) {}
void SuperPunch(bool) {}
void UnderwaterBreathing(bool) {}
void SetCycleJump(bool) {}
void SetNeverHungry(bool) {}
void SetFastSprint(bool) {}
void SetSprintEverywhere(bool) {}
void SetDrunkEffect(bool) {}
void SetNeverWanted(bool) {}
void SetGodMode(bool enable) { s_state.godMode = enable; if (!HasPersistentEffect()) RestoreSnapshot(); }
bool IsGodMode() { return s_state.godMode; }
void SetInvisible(bool enable) { s_state.invisible = enable; if (!HasPersistentEffect()) RestoreSnapshot(); }
bool IsInvisible() { return s_state.invisible; }
void SetHardMode(bool enable) { s_state.hardMode = enable; if (!HasPersistentEffect()) RestoreSnapshot(); }
bool IsHardMode() { return s_state.hardMode; }
void SetFreeFly(bool enable) { s_state.freeFly = enable; if (!HasPersistentEffect()) RestoreSnapshot(); }
bool IsFreeFly() { return s_state.freeFly; }

void CopyCoordinates() {
    void* player = Detail::PlayerBackend::GetPlayer();
    if (!player || !Detail::PlayerBackend::IsPlayerValid(player)) return;
    const auto position = Detail::PlayerBackend::GetPosition(player);
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%.3f, %.3f, %.3f", position.x, position.y, position.z);
    Platform::SetClipboardText(buffer);
}
bool RequestSaveGame() { return false; }
void MoveForward(float distance) { MoveRelative(distance, 0.0f, 0.0f); }
void MoveUp(float distance) { MoveRelative(0.0f, 0.0f, distance); }
void MoveDown(float distance) { MoveRelative(0.0f, 0.0f, -distance); }
void ApplyAimSkinChanger() {}

} // namespace XBase::Player