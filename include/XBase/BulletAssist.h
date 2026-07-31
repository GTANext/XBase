#pragma once

class CPed;

namespace XBase::BulletAssist {

void Init();
void Process();
void Draw();
bool ShouldSuppressPedFire(CPed* ped);

} // namespace XBase::BulletAssist
