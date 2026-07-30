#pragma once

namespace XBase::Visual {

void DisplayHud(bool enable);
void DisplayRadar(bool enable);
void SetFilter(int id, float strength);
void Process();

} // namespace XBase::Visual
