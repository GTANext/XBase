#pragma once

#include <XBase/Visual.h>

namespace XBase::Detail::VisualBackend {

bool DisplayHud(bool enable);
bool DisplayRadar(bool enable);
bool SetFilter(int id, float strength);
void SetRadarOptions(const Visual::RadarOptions& options);
void Init();
void Shutdown();
void Process();

} // namespace XBase::Detail::VisualBackend