#include <XBase/Ped.h>
#include <XBase/Core.h>
#include <XBase/Log.h>
#include "TypesSdk.h"
#include "plugin.h"
#include "CPlayerPed.h"
#include "CPed.h"
#include "CPools.h"
#include "CWorld.h"
#include "CVector.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CCheat.h"
#include "CGangWars.h"
#include "CGangs.h"
#include "CGangInfo.h"
#include "CZoneInfo.h"
#include "CTheZones.h"
#include "CRadar.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include <cmath>

namespace {

bool IsPedInPool(CPed* ped) {
    if (!ped || !CPools::ms_pPedPool) return false;
    for (CPed* pooledPed : CPools::ms_pPedPool) {
        if (pooledPed == ped) return true;
    }
    return false;
}

bool s_noFireEnabled = false;

bool s_elvisEverywhere = false;
bool s_everyoneArmed = false;
bool s_pedsMayhem = false;
bool s_pedsAtkRocket = false;
bool s_slutMagnet = false;
bool s_bigHead = false;
bool s_thinBody = false;
bool s_nastyLimbs = false;
bool s_noProstitutes = false;

bool s_gangsControl = false;
bool s_gangsEverywhere = false;
bool s_gangWarsActive = false;

CPed* s_lastSpawned = nullptr;

bool* CheatAddr(int index) {
    return reinterpret_cast<bool*>(0x969130 + index);
}

} // namespace

namespace XBase::Ped {

void Process() {
    *CheatAddr(65) = s_elvisEverywhere;
    *CheatAddr(22) = s_everyoneArmed;
    *CheatAddr(107) = s_pedsMayhem;
    *CheatAddr(51) = s_pedsAtkRocket;
    *CheatAddr(15) = s_slutMagnet;
    *reinterpret_cast<bool*>(0xB7CEE4) = s_bigHead;
    *reinterpret_cast<bool*>(0xB7CEE5) = s_thinBody;
    *CheatAddr(26) = s_nastyLimbs;

    *CheatAddr(80) = s_gangsControl;
    *CheatAddr(96) = s_gangsEverywhere;
    *CheatAddr(93) = s_gangWarsActive;
}

void NotifyGameInit() {
    s_lastSpawned = nullptr;
}

void Shutdown() {
    s_noFireEnabled = false;
    s_elvisEverywhere = false;
    s_everyoneArmed = false;
    s_pedsMayhem = false;
    s_pedsAtkRocket = false;
    s_slutMagnet = false;
    s_bigHead = false;
    s_thinBody = false;
    s_nastyLimbs = false;
    s_noProstitutes = false;
    s_gangsControl = false;
    s_gangsEverywhere = false;
    s_gangWarsActive = false;
    s_lastSpawned = nullptr;
}

void SetNoFire(bool enable) {
    s_noFireEnabled = enable;
}

bool GetNoFire() {
    return s_noFireEnabled;
}

void SetSpawnLimits(bool limitPolice, bool limitGangs, int maxPolice, int maxGangs) {
    if (!Core::IsWorldReady()) return;

    CPlayerPed* player = FindPlayerPed();
    if (!player || !CPools::ms_pPedPool) return;

    CVector ppos = player->GetPosition();
    int countCop = 0, countGang = 0;

    for (CPed* p : *CPools::ms_pPedPool) {
        if (!p || p == player || p->m_fHealth <= 0.0f) continue;
        if (Types::IsMissionPed(p)) continue;
        CVector pp = p->GetPosition();
        float d2 = (pp.x - ppos.x) * (pp.x - ppos.x) + (pp.y - ppos.y) * (pp.y - ppos.y) + (pp.z - ppos.z) * (pp.z - ppos.z);
        if (d2 > 120.0f * 120.0f) continue;
        if (Types::IsCopPed(p)) ++countCop;
        else if (Types::IsGangPed(p)) ++countGang;
    }

    for (CPed* p : *CPools::ms_pPedPool) {
        if (!p || p == player || p->m_fHealth <= 0.0f) continue;
        if (Types::IsMissionPed(p)) continue;
        CVector pp = p->GetPosition();
        float d2 = (pp.x - ppos.x) * (pp.x - ppos.x) + (pp.y - ppos.y) * (pp.y - ppos.y) + (pp.z - ppos.z) * (pp.z - ppos.z);
        if (d2 > 120.0f * 120.0f) continue;

        if (limitPolice && Types::IsCopPed(p) && countCop > maxPolice) {
            CWorld::Remove(p);
            delete p;
            --countCop;
        } else if (limitGangs && Types::IsGangPed(p) && countGang > maxGangs) {
            CWorld::Remove(p);
            delete p;
            --countGang;
        }
    }
}

PedId GetLastSpawnedId() {
    if (!s_lastSpawned || !IsPedInPool(s_lastSpawned)) return {};
    const int ref = CPools::GetPedRef(s_lastSpawned);
    return PedId{ref >= 0 ? static_cast<std::uint32_t>(ref) + 1u : 0u};
}

PedSnapshot GetLastSpawnedSnapshot() {
    PedSnapshot snapshot;
    if (!s_lastSpawned || !IsPedInPool(s_lastSpawned)) return snapshot;

    const CVector& position = s_lastSpawned->GetPosition();
    snapshot.valid = true;
    snapshot.id = GetLastSpawnedId();
    snapshot.modelId = static_cast<unsigned int>(s_lastSpawned->m_nModelIndex);
    snapshot.position = {position.x, position.y, position.z};
    snapshot.health = s_lastSpawned->m_fHealth;
    snapshot.armour = s_lastSpawned->m_fArmour;
    snapshot.player = s_lastSpawned == FindPlayerPed();
    snapshot.mission = Types::IsMissionPed(s_lastSpawned);
    snapshot.cop = Types::IsCopPed(s_lastSpawned);
    snapshot.gang = Types::IsGangPed(s_lastSpawned);
    return snapshot;
}

bool SpawnNearPlayer(unsigned int modelId, const Types::PedSpawnOptions& options) {
    if (!Core::IsWorldReady()) return false;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;

    if (!CModelInfo::IsPedModel(static_cast<int>(modelId))) {
        Log::Error("SpawnNearPlayer: invalid ped model id");
        return false;
    }

    CStreaming::RequestModel(static_cast<int>(modelId), 0);
    CStreaming::LoadAllRequestedModels(false);
    if (!CStreaming::HasModelLoaded(static_cast<int>(modelId))) {
        Log::Error("SpawnNearPlayer: failed to load model");
        return false;
    }

    CVector pos = player->GetPosition();
    float heading = FindPlayerHeading();
    pos.x += 3.0f * std::sin(heading);
    pos.y += 3.0f * std::cos(heading);

    int handle = -1;
    plugin::Command<plugin::Commands::CREATE_CHAR>(options.pedType, static_cast<int>(modelId), pos.x, pos.y, pos.z, &handle);
    if (handle == -1) {
        Log::Error("SpawnNearPlayer: CREATE_CHAR failed");
        return false;
    }

    CPed* ped = CPools::GetPed(handle);
    if (!ped) {
        Log::Error("SpawnNearPlayer: invalid ped handle");
        return false;
    }

    ped->m_fHealth = options.health;
    ped->m_fArmour = options.armour;

    if (options.hostile) {
        ped->m_nPedType = PED_TYPE_MISSION1;
    }

    if (options.weaponModel != 0) {
        ped->GiveWeapon(static_cast<eWeaponType>(options.weaponModel), 9999, false);
    }

    s_lastSpawned = ped;
    return true;
}

bool SpawnAtMarker(unsigned int modelId, const Types::PedSpawnOptions& options) {
    if (!Core::IsWorldReady()) return false;

    CVector markerPos;
    bool found = false;

    for (unsigned int i = 0; i < 175; i++) {
        tRadarTrace& trace = CRadar::ms_RadarTrace[i];
        if (trace.m_bInUse && trace.m_nBlipType == BLIP_COORD) {
            markerPos = trace.m_vecPos;
            found = true;
            break;
        }
    }

    if (!found) {
        Log::Error("SpawnAtMarker: no target marker found");
        return false;
    }

    if (!CModelInfo::IsPedModel(static_cast<int>(modelId))) {
        Log::Error("SpawnAtMarker: invalid ped model id");
        return false;
    }

    CStreaming::RequestModel(static_cast<int>(modelId), 0);
    CStreaming::LoadAllRequestedModels(false);
    if (!CStreaming::HasModelLoaded(static_cast<int>(modelId))) {
        Log::Error("SpawnAtMarker: failed to load model");
        return false;
    }

    int handle = -1;
    plugin::Command<plugin::Commands::CREATE_CHAR>(options.pedType, static_cast<int>(modelId), markerPos.x, markerPos.y, markerPos.z, &handle);
    if (handle == -1) {
        Log::Error("SpawnAtMarker: CREATE_CHAR failed");
        return false;
    }

    CPed* ped = CPools::GetPed(handle);
    if (!ped) {
        Log::Error("SpawnAtMarker: invalid ped handle");
        return false;
    }

    ped->m_fHealth = options.health;
    ped->m_fArmour = options.armour;

    if (options.hostile) {
        ped->m_nPedType = PED_TYPE_MISSION1;
    }

    if (options.weaponModel != 0) {
        ped->GiveWeapon(static_cast<eWeaponType>(options.weaponModel), 9999, false);
    }

    s_lastSpawned = ped;
    return true;
}

void DeleteLastSpawned() {
    if (!s_lastSpawned) return;
    CWorld::Remove(s_lastSpawned);
    delete s_lastSpawned;
    s_lastSpawned = nullptr;
}

void SetElvisEverywhere(bool enable) { s_elvisEverywhere = enable; }
bool IsElvisEverywhere() { return s_elvisEverywhere; }

void SetEveryoneArmed(bool enable) { s_everyoneArmed = enable; }
bool IsEveryoneArmed() { return s_everyoneArmed; }

void SetPedsMayhem(bool enable) { s_pedsMayhem = enable; }
bool IsPedsMayhem() { return s_pedsMayhem; }

void SetPedsAtkRocket(bool enable) { s_pedsAtkRocket = enable; }
bool IsPedsAtkRocket() { return s_pedsAtkRocket; }

void SetSlutMagnet(bool enable) { s_slutMagnet = enable; }
bool IsSlutMagnet() { return s_slutMagnet; }

void SetBigHead(bool enable) { s_bigHead = enable; }
bool IsBigHead() { return s_bigHead; }

void SetThinBody(bool enable) { s_thinBody = enable; }
bool IsThinBody() { return s_thinBody; }

void SetNastyLimbs(bool enable) { s_nastyLimbs = enable; }
bool IsNastyLimbs() { return s_nastyLimbs; }

void SetNoProstitutes(bool enable) { s_noProstitutes = enable; }
bool IsNoProstitutes() { return s_noProstitutes; }

void SetGangsControl(bool enable) { s_gangsControl = enable; }
bool IsGangsControl() { return s_gangsControl; }

void SetGangsEverywhere(bool enable) { s_gangsEverywhere = enable; }
bool IsGangsEverywhere() { return s_gangsEverywhere; }

void SetGangWarsActive(bool enable) {
    s_gangWarsActive = enable;
    if (Core::IsWorldReady()) {
        CGangWars::SetGangWarsActive(enable);
    }
}

bool IsGangWarsActive() {
    return s_gangWarsActive;
}

void StartGangWar(bool offensive) {
    if (!Core::IsWorldReady()) return;
    if (offensive)
        CGangWars::StartOffensiveGangWar();
    else
        CGangWars::StartDefensiveGangWar();
}

void EndGangWar() {
    if (!Core::IsWorldReady()) return;
    CGangWars::bGangWarsActive = false;
    CGangWars::EndGangWar(true);
}

int GetGangZoneDensity(int gangId) {
    if (gangId < 0 || gangId >= 10) return 0;
    if (!Core::IsWorldReady()) return 0;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return 0;

    CVector pos = player->GetPosition();
    CZone* zone = nullptr;
    CZoneInfo* zoneInfo = CTheZones::GetZoneInfo(&pos, &zone);
    if (!zoneInfo) return 0;

    return zoneInfo->m_nGangDensity[gangId];
}

void SetGangZoneDensity(int gangId, int density) {
    if (gangId < 0 || gangId >= 10) return;
    if (!Core::IsWorldReady()) return;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    CVector pos = player->GetPosition();
    CZone* zone = nullptr;
    CZoneInfo* zoneInfo = CTheZones::GetZoneInfo(&pos, &zone);
    if (!zoneInfo) return;

    zoneInfo->m_nGangDensity[gangId] = static_cast<char>(density);
}

unsigned int GetGangMemberModel(unsigned int gangId, unsigned int slot) {
    if (gangId >= 10) return 0;
    return static_cast<unsigned int>(CGangs::Gang[gangId].m_nPedModelOverride);
}

void SetGangMemberModel(unsigned int gangId, unsigned int slot, unsigned int modelId) {
    if (gangId >= 10) return;
    CGangs::Gang[gangId].m_nPedModelOverride = static_cast<signed char>(modelId);
}

void ResetGangModels() {
    for (int i = 0; i < 10; i++) {
        CGangs::Gang[i].m_nPedModelOverride = -1;
    }
}

void SetGangWeapons(unsigned int gangId, int weapon1, int weapon2, int weapon3) {
    if (gangId >= 10) return;
    CGangs::SetGangWeapons(static_cast<short>(gangId), weapon1, weapon2, weapon3);
}

} // namespace XBase::Ped
