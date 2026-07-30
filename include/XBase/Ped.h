#pragma once

#include "Types.h"

class CPed;

namespace XBase::Ped {

void Process();
void SetNoFire(bool enable);
bool GetNoFire();
void SetSpawnLimits(bool limitPolice, bool limitGangs, int maxPolice, int maxGangs);

CPed* GetLastSpawned();
bool SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options);
bool SpawnAtMarker(unsigned int modelId, const Types::PedSpawnOptions& options);
void DeleteLastSpawned();

void SetElvisEverywhere(bool enable);
bool IsElvisEverywhere();
void SetEveryoneArmed(bool enable);
bool IsEveryoneArmed();
void SetPedsMayhem(bool enable);
bool IsPedsMayhem();
void SetPedsAtkRocket(bool enable);
bool IsPedsAtkRocket();
void SetSlutMagnet(bool enable);
bool IsSlutMagnet();
void SetBigHead(bool enable);
bool IsBigHead();
void SetThinBody(bool enable);
bool IsThinBody();
void SetNastyLimbs(bool enable);
bool IsNastyLimbs();
void SetNoProstitutes(bool enable);
bool IsNoProstitutes();

void SetGangsControl(bool enable);
bool IsGangsControl();
void SetGangsEverywhere(bool enable);
bool IsGangsEverywhere();
void SetGangWarsActive(bool enable);
bool IsGangWarsActive();
void StartGangWar(bool offensive);
void EndGangWar();
int GetGangZoneDensity(int gangId);
void SetGangZoneDensity(int gangId, int density);
unsigned int GetGangMemberModel(unsigned int gangId, unsigned int slot);
void SetGangMemberModel(unsigned int gangId, unsigned int slot, unsigned int modelId);
void ResetGangModels();
void SetGangWeapons(unsigned int gangId, int weapon1, int weapon2, int weapon3);

} // namespace XBase::Ped
