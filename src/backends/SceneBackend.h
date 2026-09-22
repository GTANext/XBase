#pragma once

#include <XBase/Scene.h>

namespace XBase::Detail::SceneBackend {

const char* GetMissionStatus();
bool FailMission();
bool StartMission(int missionId);

} // namespace XBase::Detail::SceneBackend
