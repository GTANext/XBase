#include <XBase/Scene.h>
#include <XBase/Core.h>
#include <XBase/Log.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CCamera.h"
#include "CCutsceneMgr.h"
#include "FxManager_c.h"
#include "FxSystem_c.h"
#include "extensions/ScriptCommands.h"
#include <cstring>
#include <vector>

namespace {

std::vector<FxSystem_c*> s_particleSystems;
std::string s_activeAnimationGroup;
std::string s_pendingAnimationGroup;
DWORD s_pendingAnimationRemoveAt = 0;

struct CutsceneRestoreState {
    bool owned = false;
    int interior = 0;
    int vehicleHandle = -1;
    int vehicleSeat = -1;
};

CutsceneRestoreState s_cutsceneRestore;

void ClearCutsceneRestoreState() {
    s_cutsceneRestore = {};
    s_cutsceneRestore.vehicleHandle = -1;
    s_cutsceneRestore.vehicleSeat = -1;
}

void RestoreCutscenePlayer() {
    if (!s_cutsceneRestore.owned) return;

    CPlayerPed* player = FindPlayerPed();
    if (player) {
        player->m_nAreaCode = static_cast<unsigned char>(s_cutsceneRestore.interior);
        plugin::Command<plugin::Commands::SET_AREA_VISIBLE>(s_cutsceneRestore.interior);

        if (s_cutsceneRestore.vehicleHandle >= 0) {
            CVehicle* vehicle = CPools::GetVehicle(s_cutsceneRestore.vehicleHandle);
            if (vehicle) {
                const int playerHandle = CPools::GetPedRef(player);
                if (s_cutsceneRestore.vehicleSeat < 0) {
                    plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR>(
                        playerHandle, s_cutsceneRestore.vehicleHandle);
                } else {
                    plugin::Command<plugin::Commands::WARP_CHAR_INTO_CAR_AS_PASSENGER>(
                        playerHandle,
                        s_cutsceneRestore.vehicleHandle,
                        s_cutsceneRestore.vehicleSeat);
                }
            }
        }
        TheCamera.Fade(0.0f, 1);
    }

    ClearCutsceneRestoreState();
}

void KeepAnimationGroupLoaded(const char* group) {
    if (!group || group[0] == '\0' || std::strcmp(group, "PED") == 0) {
        return;
    }
    if (!s_activeAnimationGroup.empty() && s_activeAnimationGroup != group) {
        plugin::Command<plugin::Commands::REMOVE_ANIMATION>(s_activeAnimationGroup.c_str());
    }
    plugin::Command<plugin::Commands::REQUEST_ANIMATION>(group);
    plugin::Command<plugin::Commands::LOAD_ALL_MODELS_NOW>();
    s_activeAnimationGroup = group;
    s_pendingAnimationGroup.clear();
    s_pendingAnimationRemoveAt = 0;
}

void ScheduleAnimationGroupUnload(const char* group, DWORD delayMs) {
    if (!group || group[0] == '\0' || std::strcmp(group, "PED") == 0) {
        return;
    }
    if (s_activeAnimationGroup == group) {
        s_pendingAnimationGroup = group;
        s_pendingAnimationRemoveAt = GetTickCount() + delayMs;
    }
}

void ProcessAnimationGroupUnload() {
    if (s_pendingAnimationGroup.empty() || s_pendingAnimationRemoveAt == 0 ||
        GetTickCount() < s_pendingAnimationRemoveAt) {
        return;
    }
    if (s_activeAnimationGroup == s_pendingAnimationGroup) {
        plugin::Command<plugin::Commands::REMOVE_ANIMATION>(s_pendingAnimationGroup.c_str());
        s_activeAnimationGroup.clear();
    }
    s_pendingAnimationGroup.clear();
    s_pendingAnimationRemoveAt = 0;
}

} // namespace

namespace XBase::Scene {

void NotifyGameInit() {
    s_particleSystems.clear();
    s_activeAnimationGroup.clear();
    s_pendingAnimationGroup.clear();
    s_pendingAnimationRemoveAt = 0;
    ClearCutsceneRestoreState();
}

void Shutdown() {
    if (s_cutsceneRestore.owned) {
        CCutsceneMgr::DeleteCutsceneData();
        RestoreCutscenePlayer();
    }
    for (FxSystem_c* sys : s_particleSystems) {
        if (sys) {
            g_fxMan.DestroyFxSystem(sys);
        }
    }
    s_particleSystems.clear();
    if (!s_activeAnimationGroup.empty()) {
        plugin::Command<plugin::Commands::REMOVE_ANIMATION>(s_activeAnimationGroup.c_str());
    }
    s_activeAnimationGroup.clear();
    s_pendingAnimationGroup.clear();
    s_pendingAnimationRemoveAt = 0;
}

void Process() {
    ProcessAnimationGroupUnload();
    if (s_cutsceneRestore.owned && !IsCutsceneRunning()) {
        RestoreCutscenePlayer();
    }
    for (size_t i = 0; i < s_particleSystems.size(); ) {
        FxSystem_c* sys = s_particleSystems[i];
        if (!sys || sys->m_nPlayStatus == 3) {
            s_particleSystems.erase(s_particleSystems.begin() + static_cast<int>(i));
        } else {
            ++i;
        }
    }
}

bool PlayAnimation(const char* group, const char* name, bool loop) {
    AnimationOptions options;
    options.loop = loop;
    return PlayAnimation(group, name, options);
}

bool PlayAnimation(const char* group, const char* name, const AnimationOptions& options) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !group || !name || group[0] == '\0' || name[0] == '\0') return false;

    CPed* target = player;
    if (options.onTargetPed) {
        target = player->m_pPlayerTargettedPed;
        if (!target || !CPools::ms_pPedPool || !CPools::ms_pPedPool->IsObjectValid(target)) {
            return false;
        }
    }

    if (std::strcmp(group, "PED") != 0) {
        KeepAnimationGroupLoaded(group);
    }
    const int targetHandle = CPools::GetPedRef(target);
    const int flags = options.loop ? 1 : 0;
    if (options.secondary) {
        plugin::Command<plugin::Commands::TASK_PLAY_ANIM_SECONDARY>(
            targetHandle, name, group, 8.0f, flags, 0, 0, 0, 0);
    } else {
        plugin::Command<plugin::Commands::TASK_PLAY_ANIM>(
            targetHandle, name, group, 8.0f, flags, 0, 0, 0, 0);
    }
    if (std::strcmp(group, "PED") != 0) {
        ScheduleAnimationGroupUnload(group, options.loop ? 60000 : 8000);
    }
    return true;
}

bool StopAnimation() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<plugin::Commands::CLEAR_CHAR_TASKS>(hplayer);
    return true;
}

bool PlayParticle(const char* name) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !name) return false;
    CVector pos = player->GetPosition();
    FxSystem_c* sys = g_fxMan.CreateFxSystem(const_cast<char*>(name), &pos, nullptr, false);
    if (sys) {
        s_particleSystems.push_back(sys);
    }
    return sys != nullptr;
}

bool RemoveAllParticles() {
    if (s_particleSystems.empty()) return false;
    for (FxSystem_c* sys : s_particleSystems) {
        if (sys) {
            g_fxMan.DestroyFxSystem(sys);
        }
    }
    s_particleSystems.clear();
    return true;
}

bool RemoveLatestParticle() {
    if (s_particleSystems.empty()) return false;
    FxSystem_c* sys = s_particleSystems.back();
    s_particleSystems.pop_back();
    if (sys) {
        g_fxMan.DestroyFxSystem(sys);
    }
    return true;
}

bool StartCutscene(const char* name) {
    return StartCutscene(name, 0);
}

bool StartCutscene(const char* name, int interior) {
    if (!name || !name[0] || IsCutsceneRunning() || s_cutsceneRestore.owned) return false;

    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;

    ClearCutsceneRestoreState();
    s_cutsceneRestore.owned = true;
    s_cutsceneRestore.interior = player->m_nAreaCode;

    CVehicle* vehicle = player->bInVehicle ? player->m_pVehicle : nullptr;
    if (vehicle) {
        s_cutsceneRestore.vehicleHandle = CPools::GetVehicleRef(vehicle);
        if (vehicle->m_pDriver != player) {
            for (int seat = 0; seat < 8; ++seat) {
                if (vehicle->m_apPassengers[seat] == player) {
                    s_cutsceneRestore.vehicleSeat = seat;
                    break;
                }
            }
        }
    }

    CCutsceneMgr::DeleteCutsceneData();
    CCutsceneMgr::LoadCutsceneData(name);
    CCutsceneMgr::Update();
    player->m_nAreaCode = static_cast<unsigned char>(interior);
    plugin::Command<plugin::Commands::SET_AREA_VISIBLE>(interior);
    CCutsceneMgr::StartCutscene();
    return true;
}

bool StopCutscene() {
    if (!s_cutsceneRestore.owned) return false;
    CCutsceneMgr::DeleteCutsceneData();
    RestoreCutscenePlayer();
    return true;
}

bool IsCutsceneRunning() {
    return CCutsceneMgr::ms_running
        || CCutsceneMgr::ms_cutsceneProcessing
        || (s_cutsceneRestore.owned && CCutsceneMgr::ms_cutsceneLoadStatus != 0);
}

const char* GetMissionStatus() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return "No Player";
    int status = *reinterpret_cast<int*>(0xC8D4C0);
    switch (status) {
        case 0: return "No Mission";
        case 1: return "On Mission";
        case 2: return "Passed";
        case 3: return "Failed";
        default: return "Unknown";
    }
}

bool FailMission() {
    if (!FindPlayerPed()) return false;
    *reinterpret_cast<int*>(0xC8D4C0) = 3;
    *reinterpret_cast<bool*>(0x96918C) = true;
    return true;
}

bool StartMission(int missionId) {
    if (missionId < 0) return false;

    CPlayerPed* player = FindPlayerPed();
    if (!player || !player->CanPlayerStartMission()) return false;

    // 室内直接载入任务脚本会把玩家留在错误区域
    int areaVisible = 0;
    plugin::Command<plugin::Commands::GET_CHAR_AREA_VISIBLE>(CPools::GetPedRef(player), &areaVisible);
    if (areaVisible != 0) return false;

    // 通缉状态会跟随任务脚本 先清空再载入
    player->SetWantedLevel(0);
    plugin::Command<plugin::Commands::LOAD_AND_LAUNCH_MISSION_INTERNAL>(missionId);
    return true;
}

bool SetFightingStyle(int style) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<0x0730>(hplayer, style);
    return true;
}

bool SetWalkingStyle(int style) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<0x0747>(hplayer, style);
    return true;
}

} // namespace XBase::Scene