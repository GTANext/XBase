#include <XBase/Ped.h>
#include <XBase/Core.h>
#include "PedBackend.h"
#include <deque>

namespace XBase::Ped {
namespace {
void* s_lastSpawned = nullptr;
bool s_noFire = false;
bool s_elvis = false;
bool s_armed = false;
bool s_mayhem = false;
bool s_atkRocket = false;
bool s_riot = false;
bool s_slutMagnet = false;
bool s_gangsControl = false;
bool s_gangsEverywhere = false;
bool s_noProstitutes = false;
bool s_nastyLimbs = false;
bool s_gangWars = false;
bool s_limitPolice = false;
bool s_limitGangs = false;
int s_maxPolice = 0;
int s_maxGangs = 0;

void ApplyGlobalOptions() {
    Detail::PedBackend::SetElvisEverywhere(s_elvis);
    Detail::PedBackend::SetEveryoneArmed(s_armed);
    Detail::PedBackend::SetPedsMayhem(s_mayhem);
    Detail::PedBackend::SetPedsAtkRocket(s_atkRocket);
    Detail::PedBackend::SetPedsRiot(s_riot);
    Detail::PedBackend::SetSlutMagnet(s_slutMagnet);
    Detail::PedBackend::SetGangsControl(s_gangsControl);
    Detail::PedBackend::SetGangsEverywhere(s_gangsEverywhere);
    Detail::PedBackend::SetNoProstitutes(s_noProstitutes);
    Detail::PedBackend::SetNastyLimbs(s_nastyLimbs);
    Detail::PedBackend::SetGangWarsActive(s_gangWars);
}
}

void Process() {
    if (!Core::IsWorldReady()) return;
    ApplyGlobalOptions();
}

void NotifyGameInit() {
    s_lastSpawned = nullptr;
}

void Shutdown() {
    s_lastSpawned = nullptr;
    s_noFire = false;
    s_limitPolice = false;
    s_limitGangs = false;
    s_maxPolice = 0;
    s_maxGangs = 0;
    s_elvis = false;
    s_armed = false;
    s_mayhem = false;
    s_atkRocket = false;
    s_riot = false;
    s_slutMagnet = false;
    s_gangsControl = false;
    s_gangsEverywhere = false;
    s_noProstitutes = false;
    s_nastyLimbs = false;
    s_gangWars = false;
}
void SetNoFire(bool enable) { s_noFire = enable; }
bool GetNoFire() { return s_noFire; }
void SetSpawnLimits(bool limitPolice, bool limitGangs, int maxPolice, int maxGangs) {
    s_limitPolice = limitPolice;
    s_limitGangs = limitGangs;
    s_maxPolice = maxPolice < 0 ? 0 : maxPolice;
    s_maxGangs = maxGangs < 0 ? 0 : maxGangs;
}
PedId GetLastSpawnedId() {
    const int ref = Detail::PedBackend::GetId(s_lastSpawned);
    return PedId{ref >= 0 ? static_cast<std::uint32_t>(ref) + 1u : 0u};
}
PedSnapshot GetLastSpawnedSnapshot() {
    PedSnapshot snapshot;
    if (!Detail::PedBackend::IsValid(s_lastSpawned)) return snapshot;

    snapshot.valid = true;
    snapshot.id = GetLastSpawnedId();
    snapshot.modelId = Detail::PedBackend::GetModelId(s_lastSpawned);
    snapshot.position = Detail::PedBackend::GetPosition(s_lastSpawned);
    snapshot.health = Detail::PedBackend::GetHealth(s_lastSpawned);
    snapshot.armour = Detail::PedBackend::GetArmour(s_lastSpawned);
    snapshot.player = Detail::PedBackend::IsPlayer(s_lastSpawned);
    snapshot.mission = Detail::PedBackend::IsMission(s_lastSpawned);
    snapshot.cop = Detail::PedBackend::IsCop(s_lastSpawned);
    snapshot.gang = Detail::PedBackend::IsGang(s_lastSpawned);
    return snapshot;
}
bool SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options) {
    s_lastSpawned = Detail::PedBackend::SpawnNearPlayer(modelId, options);
    return s_lastSpawned != nullptr;
}
bool SpawnAtMarker(unsigned int modelId, const Types::PedSpawnOptions& options) {
    s_lastSpawned = Detail::PedBackend::SpawnAtMarker(modelId, options);
    return s_lastSpawned != nullptr;
}
void DeleteLastSpawned() {
    Detail::PedBackend::Delete(s_lastSpawned);
    s_lastSpawned = nullptr;
}
#define PED_FLAG(name, field) \
    void Set##name(bool enable) { field = enable; } \
    bool Is##name() { return field; }
PED_FLAG(ElvisEverywhere, s_elvis)
PED_FLAG(EveryoneArmed, s_armed)
PED_FLAG(PedsMayhem, s_mayhem)
PED_FLAG(PedsAtkRocket, s_atkRocket)
PED_FLAG(SlutMagnet, s_slutMagnet)
PED_FLAG(GangsControl, s_gangsControl)
PED_FLAG(GangsEverywhere, s_gangsEverywhere)
PED_FLAG(NoProstitutes, s_noProstitutes)
PED_FLAG(NastyLimbs, s_nastyLimbs)
#undef PED_FLAG
void SetBigHead(bool) {}
bool IsBigHead() { return false; }
void SetThinBody(bool) {}
bool IsThinBody() { return false; }
void SetPedsRiot(bool enable) { s_riot = enable; }
bool IsPedsRiot() { return s_riot; }
void SetGangWarsActive(bool enable) { s_gangWars = enable; }
bool IsGangWarsActive() { return s_gangWars; }
void StartGangWar(bool offensive) { Detail::PedBackend::StartGangWar(offensive); }
void EndGangWar() { Detail::PedBackend::EndGangWar(); }
int GetGangZoneDensity(int id) { return Detail::PedBackend::GetGangZoneDensity(id); }
void SetGangZoneDensity(int id, int density) { Detail::PedBackend::SetGangZoneDensity(id, density); }
unsigned int GetGangMemberModel(unsigned int gang, unsigned int slot) { return Detail::PedBackend::GetGangMemberModel(gang, slot); }
void SetGangMemberModel(unsigned int gang, unsigned int slot, unsigned int model) { Detail::PedBackend::SetGangMemberModel(gang, slot, model); }
void ResetGangModels() { Detail::PedBackend::ResetGangModels(); }
void SetGangWeapons(unsigned int gang, int one, int two, int three) { Detail::PedBackend::SetGangWeapons(gang, one, two, three); }

} // namespace XBase::Ped