#pragma once

namespace XBase::Detail::RuntimeGuard {

// Installs the game file directory and file handle guards where the target
// version needs them. Repeated calls keep a single hook set.
bool Init();

// Removes every hook installed by Init and resets the safety timer.
void Shutdown();

// True only while the game is in a state where object access during bullet
// assist work is safe. Unsafe states include cutscenes, script camera or
// world transitions, disabled player control and a dead or unavailable player.
bool IsRuntimeSafe();

} // namespace XBase::Detail::RuntimeGuard
