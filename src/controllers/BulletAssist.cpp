#include <XBase/BulletAssist.h>
#include <XBase/Core.h>
#include <XBase/Ped.h>
#include "../backends/BulletAssistBackend.h"

namespace XBase::BulletAssist {
namespace {
Config s_config;
bool s_initialized = false;
}

void SetConfig(const Config& config) {
    s_config = config;
    if (s_config.lockRange < 10.0f) s_config.lockRange = 10.0f;
    if (s_config.maxTargets < 1) s_config.maxTargets = 1;
    if (s_config.maxTargets > 16) s_config.maxTargets = 16;
}

Config GetConfig() {
    return s_config;
}

void Init() {
    if (s_initialized) return;
    s_initialized = Detail::BulletAssistBackend::Init();
}

bool IsInitialized() {
    return s_initialized;
}

void Process() {
    if (!s_initialized || !Core::IsWorldReady()) return;
    Detail::BulletAssistBackend::Process(s_config);
}

void Shutdown() {
    if (s_initialized) Detail::BulletAssistBackend::Shutdown();
    s_initialized = false;
    s_config = {};
}

void Draw() {
    if (!s_initialized || !Core::IsWorldReady()) return;
    Detail::BulletAssistBackend::Draw(s_config);
}

bool ShouldSuppressPedFire(PedId ped) {
    return Detail::BulletAssistBackend::ShouldSuppressPedFire(ped, Ped::GetNoFire());
}

} // namespace XBase::BulletAssist