#include <XBase/Scene.h>
#include <XBase/Core.h>
#include <XBase/Log.h>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CCutsceneMgr.h"
#include "FxManager_c.h"
#include "FxSystem_c.h"
#include "extensions/ScriptCommands.h"
#include <vector>

namespace {

std::vector<FxSystem_c*> s_particleSystems;

} // namespace

namespace XBase::Scene {

void Process() {
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
    CPlayerPed* player = FindPlayerPed();
    if (!player || !group || !name) return false;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<plugin::Commands::REQUEST_ANIMATION>(group);
    plugin::Command<plugin::Commands::LOAD_ALL_MODELS_NOW>();
    const int flags = loop ? 1 : 0;
    plugin::Command<plugin::Commands::TASK_PLAY_ANIM>(hplayer, name, group, 8.0f, flags, 0, 0, 0, 0);
    return true;
}

void StopAnimation() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<plugin::Commands::CLEAR_CHAR_TASKS>(hplayer);
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

void RemoveAllParticles() {
    for (FxSystem_c* sys : s_particleSystems) {
        if (sys) {
            g_fxMan.DestroyFxSystem(sys);
        }
    }
    s_particleSystems.clear();
}

void RemoveLatestParticle() {
    if (s_particleSystems.empty()) return;
    FxSystem_c* sys = s_particleSystems.back();
    s_particleSystems.pop_back();
    if (sys) {
        g_fxMan.DestroyFxSystem(sys);
    }
}

bool StartCutscene(const char* name) {
    if (!name) return false;
    CCutsceneMgr::DeleteCutsceneData();
    CCutsceneMgr::LoadCutsceneData(name);
    CCutsceneMgr::StartCutscene();
    return true;
}

void StopCutscene() {
    CCutsceneMgr::DeleteCutsceneData();
}

bool IsCutsceneRunning() {
    return CCutsceneMgr::ms_running || CCutsceneMgr::ms_cutsceneProcessing;
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

void FailMission() {
    *reinterpret_cast<int*>(0xC8D4C0) = 3;
    *reinterpret_cast<bool*>(0x96918C) = true;
}

void StartMission(int missionId) {
    if (missionId < 0) return;
    plugin::Command<plugin::Commands::LOAD_AND_LAUNCH_MISSION_INTERNAL>(missionId);
}

void SetFightingStyle(int style) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<0x0730>(hplayer, style);
}

void SetWalkingStyle(int style) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;
    const int hplayer = CPools::GetPedRef(player);
    plugin::Command<0x0747>(hplayer, style);
}

} // namespace XBase::Scene
