#pragma once

#include <XBase/Types.h>

namespace XBase::Detail::PedBackend {

void* GetPlayer();
bool IsValid(void* ped);
bool IsValidModel(unsigned int modelId);
bool IsMission(void* ped);
bool IsCop(const void* ped);
bool IsGang(const void* ped);
bool IsPlayer(void* ped);
void ClearAiming(void* ped);
void Delete(void* ped);
void ApplyOptions(void* ped, const Types::PedSpawnOptions& options);
void* SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options);
void* SpawnAtMarker(unsigned int modelId, const Types::PedSpawnOptions& options);
void SetElvisEverywhere(bool enable);
void SetEveryoneArmed(bool enable);
void SetPedsMayhem(bool enable);
void SetPedsAtkRocket(bool enable);
void SetPedsRiot(bool enable);
void SetSlutMagnet(bool enable);
void SetGangsControl(bool enable);
void SetGangsEverywhere(bool enable);
void SetNoProstitutes(bool enable);
void SetNastyLimbs(bool enable);
void SetGangWarsActive(bool enable);
void StartGangWar(bool offensive);
void EndGangWar();
int GetGangZoneDensity(int gangId);
void SetGangZoneDensity(int gangId, int density);
unsigned int GetGangMemberModel(unsigned int gangId, unsigned int slot);
void SetGangMemberModel(unsigned int gangId, unsigned int slot, unsigned int modelId);
void ResetGangModels();
void SetGangWeapons(unsigned int gangId, int weapon1, int weapon2, int weapon3);

} // namespace XBase::Detail::PedBackend