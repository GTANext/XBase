#include <XBase/Visual.h>

#include "../backends/VisualBackend.h"

namespace XBase::Visual {

bool DisplayHud(bool enable) {
    return Detail::VisualBackend::DisplayHud(enable);
}

bool DisplayRadar(bool enable) {
    return Detail::VisualBackend::DisplayRadar(enable);
}

bool SetFilter(int id, float strength) {
    return Detail::VisualBackend::SetFilter(id, strength);
}

void SetRadarOptions(const RadarOptions& options) {
    Detail::VisualBackend::SetRadarOptions(options);
}

void NotifyGameInit() {
    Detail::VisualBackend::Init();
}

void Shutdown() {
    Detail::VisualBackend::Shutdown();
}

void Process() {
    Detail::VisualBackend::Process();
}

} // namespace XBase::Visual