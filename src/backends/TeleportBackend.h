#pragma once

#include <XBase/ValueTypes.h>

namespace XBase::Detail::TeleportBackend {

bool GetPlayerPosition(Vec3& position);
bool To(const Vec3& position, int interior);
bool Forward(float distance);
bool MapPosition(const Vec3& position, bool spawnUnderwater);
bool Marker(bool spawnUnderwater);
bool Center();

} // namespace XBase::Detail::TeleportBackend