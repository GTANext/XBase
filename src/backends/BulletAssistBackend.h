#pragma once

#include <XBase/BulletAssist.h>
#include <XBase/Ped.h>

namespace XBase::Detail::BulletAssistBackend {

bool Init();
void Process(const BulletAssist::Config& config);
void Shutdown();
void Draw(const BulletAssist::Config& config);
bool ShouldSuppressPedFire(PedId ped, const Ped::NoFireOptions& options);

// 分类开关优先级为任务 警察 帮派 平民 命中即决定是否抑制
inline bool ShouldSuppressNoFire(const Ped::NoFireOptions& options, bool mission, bool cop, bool gang) {
    if (!options.enable) return false;
    if (mission) return options.mission;
    if (cop) return options.cops;
    if (gang) return options.gangs;
    return options.civilians;
}

} // namespace XBase::Detail::BulletAssistBackend