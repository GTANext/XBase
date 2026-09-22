#pragma once

#include <XBase/Teleport.h>

namespace XBase::Detail::TeleportBackend {

bool GetPlayerPosition(Vec3& position);
bool To(const Vec3& position, int interior);
bool Forward(float distance);
bool MapPosition(const Vec3& position, bool spawnUnderwater);
bool Marker(bool spawnUnderwater);
bool Center();
bool GetMapBounds(MapBounds& bounds);

} // namespace XBase::Detail::TeleportBackend