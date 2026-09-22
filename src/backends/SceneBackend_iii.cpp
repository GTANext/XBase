#include "SceneBackend.h"

#include "CPlayerPed.h"
#include "CTheScripts.h"
#include "extensions/ScriptCommands.h"
#include "plugin.h"

namespace XBase::Detail::SceneBackend {

const char* GetMissionStatus() {
    return CTheScripts::OnAMissionFlag != 0 ? "On Mission" : "No Mission";
}

bool FailMission() {
    plugin::Command<plugin::Commands::FAIL_CURRENT_MISSION>();
    return true;
}

bool StartMission(int missionId) {
    if (missionId < 0) return false;
    CPlayerPed* player = FindPlayerPed();
    if (!player || CTheScripts::OnAMissionFlag != 0) return false;
    player->SetWantedLevel(0);
    plugin::Command<plugin::Commands::LOAD_AND_LAUNCH_MISSION_INTERNAL>(missionId);
    return true;
}

} // namespace XBase::Detail::SceneBackend
