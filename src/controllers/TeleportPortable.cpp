#include <XBase/Teleport.h>

#include "../backends/TeleportBackend.h"

namespace XBase::Teleport {

bool TryGetCurrentPosition(Vec3& position) {
    return Detail::TeleportBackend::GetPlayerPosition(position);
}

Vec3 GetCurrentPosition() {
    Vec3 position{};
    Detail::TeleportBackend::GetPlayerPosition(position);
    return position;
}

bool To(float x, float y, float z, int interior) {
    return Detail::TeleportBackend::To({ x, y, z }, interior);
}

bool Forward(float distance) {
    return Detail::TeleportBackend::Forward(distance);
}

bool MapPosition(float x, float y, bool spawnUnderwater) {
    return Detail::TeleportBackend::MapPosition({ x, y, 0.0f }, spawnUnderwater);
}

bool Marker(bool spawnUnderwater) {
    return Detail::TeleportBackend::Marker(spawnUnderwater);
}

bool Center() {
    return Detail::TeleportBackend::Center();
}

void Process() {}

} // namespace XBase::Teleport