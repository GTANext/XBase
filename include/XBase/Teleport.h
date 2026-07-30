#pragma once

#include "CVector.h"

namespace XBase::Teleport {

void To(float x, float y, float z, int interior = 0);
void Forward(float distance);
void MapPosition(float x, float y, bool spawnUnderwater = false);
void Marker(bool spawnUnderwater = false);
void Center();
void Process();

} // namespace XBase::Teleport
