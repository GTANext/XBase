#pragma once

#include <XBase/Types.h>

namespace XBase::Detail::PlayerBackend {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

void* GetPlayer();
bool IsPlayerValid(void* player);
float GetHealth(void* player);
void SetHealth(void* player, float value);
float GetArmour(void* player);
void SetArmour(void* player, float value);
int GetWantedLevel(void* player);
void SetWantedLevel(void* player, int level);
int GetMoney();
void SetMoney(int amount);
Types::ProofState GetProofState(void* player);
void SetProofState(void* player, const Types::ProofState& state);
bool GetVisible(void* player);
void SetVisible(void* player, bool visible);
bool MoveRelative(void* player, float forward, float right, float up);
Position GetPosition(void* player);
bool Teleport(void* player, const Position& position);
bool GetFreeHealthcare();
void SetFreeHealthcare(bool enable);
bool GetFreeJail();
void SetFreeJail(bool enable);
bool GetInfiniteSprint();
void SetInfiniteSprint(bool enable);

} // namespace XBase::Detail::PlayerBackend