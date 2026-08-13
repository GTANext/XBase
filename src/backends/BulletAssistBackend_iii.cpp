#include "BulletAssistBackend.h"

namespace XBase::Detail::BulletAssistBackend {

bool Init() { return false; }
void Process(const BulletAssist::Config&) {}
void Shutdown() {}
void Draw(const BulletAssist::Config&) {}
bool ShouldSuppressPedFire(PedId, bool) { return false; }

} // namespace XBase::Detail::BulletAssistBackend