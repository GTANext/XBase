#include <XBase/Scene.h>

#include "../backends/SceneBackend.h"

namespace XBase::Scene {

// 动画 粒子与过场在 VC/III 仍无实现 这里只补任务状态与启动
bool PlayAnimation(const char*, const char*, bool) { return false; }
bool PlayAnimation(const char*, const char*, const AnimationOptions&) { return false; }
bool StopAnimation() { return false; }
bool PlayParticle(const char*) { return false; }
bool RemoveAllParticles() { return false; }
bool RemoveLatestParticle() { return false; }
bool StartCutscene(const char*) { return false; }
bool StartCutscene(const char*, int) { return false; }
bool StopCutscene() { return false; }
bool IsCutsceneRunning() { return false; }

const char* GetMissionStatus() {
    return Detail::SceneBackend::GetMissionStatus();
}

bool FailMission() {
    return Detail::SceneBackend::FailMission();
}

bool StartMission(int missionId) {
    return Detail::SceneBackend::StartMission(missionId);
}

bool SetFightingStyle(int) { return false; }
bool SetWalkingStyle(int) { return false; }

void Process() {}
void NotifyGameInit() {}
void Shutdown() {}

} // namespace XBase::Scene
