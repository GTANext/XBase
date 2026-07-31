#pragma once

namespace XBase::Scene {

void Process();
void Shutdown();
bool PlayAnimation(const char* group, const char* name, bool loop);
void StopAnimation();
bool PlayParticle(const char* name);
void RemoveAllParticles();
void RemoveLatestParticle();
bool StartCutscene(const char* name);
void StopCutscene();
bool IsCutsceneRunning();

const char* GetMissionStatus();
void FailMission();
void StartMission(int missionId);
void SetFightingStyle(int style);
void SetWalkingStyle(int style);

} // namespace XBase::Scene
