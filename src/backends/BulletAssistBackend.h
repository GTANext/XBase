#pragma once

#include <XBase/BulletAssist.h>

namespace XBase::Detail::BulletAssistBackend {

bool Init();
void Process(const BulletAssist::Config& config);
void Shutdown();
void Draw(const BulletAssist::Config& config);
bool ShouldSuppressPedFire(PedId ped, bool noFireEnabled);

} // namespace XBase::Detail::BulletAssistBackend