#include "RuntimeGuard.h"

#include <XBase/Core.h>

#include "common.h"
#include "CCamera.h"
#include "CCutsceneMgr.h"
#include "CDirectory.h"
#include "CFileMgr.h"
#include "CPad.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CTimer.h"
#include "kiero/minhook/MinHook.h"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace XBase::Detail::RuntimeGuard {
namespace {

using ReadDirFileFn = void(__fastcall*)(CDirectory*, void*, const char*);
using FileReadFn = int(__cdecl*)(int, char*, int);
using FileCloseFn = int(__cdecl*)(int);

constexpr std::uintptr_t kReadDirFileAddress = 0x487370;
constexpr std::uintptr_t kFileReadAddress = 0x48DF50;
constexpr std::uintptr_t kFileCloseAddress = 0x48DEA0;
constexpr unsigned int kResumeDelayMs = 750;

ReadDirFileFn s_originalReadDirFile = nullptr;
FileReadFn s_originalFileRead = nullptr;
FileCloseFn s_originalFileClose = nullptr;
bool s_ownsReadDirFile = false;
bool s_ownsFileRead = false;
bool s_ownsFileClose = false;
unsigned int s_lastUnsafeTick = 0;

bool PathExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string JoinPath(const char* base, const char* name) {
    if (!base || !base[0]) return name;
    std::string result = base;
    if (result.back() != '\\' && result.back() != '/') result += '\\';
    result += name;
    return result;
}

bool IsAbsolutePath(const char* path) {
    if (!path || !path[0]) return true;
    if (path[0] == '\\' || path[0] == '/') return true;
    return path[1] == ':';
}

void CorrectDirectoryFor(const char* filename) {
    if (IsAbsolutePath(filename)) return;

    const char* currentDirectory = CFileMgr::ms_dirName;
    const char* rootDirectory = CFileMgr::ms_rootDirName;
    if (!rootDirectory || !rootDirectory[0]) return;

    if (PathExists(JoinPath(currentDirectory, filename))) return;
    if (!PathExists(JoinPath(rootDirectory, filename))) return;
    CFileMgr::SetDir("");
}

void __fastcall HookReadDirFile(CDirectory* directory, void*, const char* filename) {
    CorrectDirectoryFor(filename);
    if (s_originalReadDirFile) s_originalReadDirFile(directory, nullptr, filename);
}

int __cdecl HookFileRead(int fileHandle, char* buffer, int size) {
    if (!fileHandle) return 0;
    return s_originalFileRead ? s_originalFileRead(fileHandle, buffer, size) : 0;
}

int __cdecl HookFileClose(int fileHandle) {
    if (!fileHandle) return 0;
    return s_originalFileClose ? s_originalFileClose(fileHandle) : 0;
}

bool InstallHook(std::uintptr_t address, void* detour, void** original, bool& owned) {
    void* target = reinterpret_cast<void*>(address);
    if (MH_CreateHook(target, detour, original) != MH_OK) return false;
    const MH_STATUS enabled = MH_EnableHook(target);
    owned = enabled == MH_OK || enabled == MH_ERROR_ENABLED;
    if (!owned) MH_RemoveHook(target);
    return owned;
}

void RemoveHook(std::uintptr_t address, bool& owned) {
    if (!owned) return;
    void* target = reinterpret_cast<void*>(address);
    MH_DisableHook(target);
    MH_RemoveHook(target);
    owned = false;
}

bool IsUnsafeWorldState() {
    if (!Core::IsWorldReady()) return true;
    if (CCutsceneMgr::ms_running || CCutsceneMgr::ms_numCutsceneObjs > 0) return true;
    if (TheCamera.m_bWideScreenOn || TheCamera.m_bStartingSpline) return true;
    if (!TheCamera.m_pRwCamera) return true;

    CPad* pad = CPad::GetPad(0);
    if (!pad || pad->DisablePlayerControls != 0) return true;

    if (!CPools::ms_pPedPool) return true;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return true;
    if (player->m_fHealth <= 0.0f) return true;
    if (player->m_ePedState == PEDSTATE_DIE || player->m_ePedState == PEDSTATE_DEAD) return true;
    return false;
}

} // namespace

bool Init() {
    if (s_ownsReadDirFile && s_ownsFileRead && s_ownsFileClose) return true;

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;

    InstallHook(kReadDirFileAddress, reinterpret_cast<void*>(&HookReadDirFile),
        reinterpret_cast<void**>(&s_originalReadDirFile), s_ownsReadDirFile);
    InstallHook(kFileReadAddress, reinterpret_cast<void*>(&HookFileRead),
        reinterpret_cast<void**>(&s_originalFileRead), s_ownsFileRead);
    InstallHook(kFileCloseAddress, reinterpret_cast<void*>(&HookFileClose),
        reinterpret_cast<void**>(&s_originalFileClose), s_ownsFileClose);

    s_lastUnsafeTick = 0;
    return s_ownsReadDirFile && s_ownsFileRead && s_ownsFileClose;
}

void Shutdown() {
    RemoveHook(kFileCloseAddress, s_ownsFileClose);
    RemoveHook(kFileReadAddress, s_ownsFileRead);
    RemoveHook(kReadDirFileAddress, s_ownsReadDirFile);
    s_originalReadDirFile = nullptr;
    s_originalFileRead = nullptr;
    s_originalFileClose = nullptr;
    s_lastUnsafeTick = 0;
}

bool IsRuntimeSafe() {
    const unsigned int now = static_cast<unsigned int>(CTimer::m_snTimeInMilliseconds);
    if (IsUnsafeWorldState()) {
        s_lastUnsafeTick = now;
        return false;
    }
    if (s_lastUnsafeTick != 0 && now - s_lastUnsafeTick < kResumeDelayMs) return false;
    return true;
}

} // namespace XBase::Detail::RuntimeGuard
