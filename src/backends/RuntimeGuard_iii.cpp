#include "RuntimeGuard.h"

namespace XBase::Detail::RuntimeGuard {

bool Init() {
    return true;
}

void Shutdown() {
}

bool IsRuntimeSafe() {
    return true;
}

} // namespace XBase::Detail::RuntimeGuard
