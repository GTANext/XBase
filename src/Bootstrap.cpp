#include "Bootstrap.h"

#include <cstdio>
#include <string>
#include <string>
#include <windows.h>

namespace XBase::Bootstrap {
namespace {

enum class DetectedGame {
    Unknown,
    SanAndreas,
    ViceCity,
    III,
};

HMODULE payloadModule = nullptr;

bool ReadUInt(std::uintptr_t address, unsigned int& value) {
    __try {
        value = *reinterpret_cast<const unsigned int*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0;
        return false;
    }
}

bool MatchUInt(std::uintptr_t address, unsigned int expected) {
    unsigned int value = 0;
    return ReadUInt(address, value) && value == expected;
}

DetectedGame DetectGame() {
    if (MatchUInt(0x401000, 0x53EC8B55) || MatchUInt(0x401000, 0x16197BE9)
        || MatchUInt(0x8245BC, 0x94BF) || MatchUInt(0x8252FC, 0x94BF)
        || MatchUInt(0x82533C, 0x94BF) || MatchUInt(0x858D51, 0x3539F633)
        || MatchUInt(0x858C61, 0x3539F633)) {
        return DetectedGame::SanAndreas;
    }

    if (MatchUInt(0x667BF0, 0x53E58955) || MatchUInt(0x667C40, 0x53E58955)
        || MatchUInt(0x666BA0, 0x53E58955)) {
        return DetectedGame::ViceCity;
    }

    if (MatchUInt(0x5C1E70, 0x53E58955) || MatchUInt(0x5C2130, 0x53E58955)
        || MatchUInt(0x5C6FD0, 0x53E58955)) {
        return DetectedGame::III;
    }

    return DetectedGame::Unknown;
}

const char* PayloadFileName(DetectedGame game) {
    switch (game) {
    case DetectedGame::SanAndreas:
        return "XMenuSA.dll";
    case DetectedGame::ViceCity:
        return "XMenuVC.dll";
    case DetectedGame::III:
        return "XMenuIII.dll";
    case DetectedGame::Unknown:
    default:
        return nullptr;
    }
}

std::string DirectoryFromModule(HMODULE module) {
    std::string path(MAX_PATH, '\0');
    for (;;) {
        const DWORD size = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0) return {};
        if (size < path.size() - 1) {
            path.resize(size);
            break;
        }
        path.resize(path.size() * 2);
    }

    const std::size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return {};
    return path.substr(0, slash + 1);
}

std::string PayloadPath(HMODULE loaderModule, DetectedGame game) {
    const char* fileName = PayloadFileName(game);
    if (!fileName) return {};

    const std::string directory = DirectoryFromModule(loaderModule);
    if (directory.empty()) return std::string("XMenu\\") + fileName;
    return directory + "XMenu\\" + fileName;
}

void ShowError(const char* message) {
    MessageBoxA(HWND_DESKTOP, message, "XMenu", MB_OK | MB_ICONERROR);
}

} // namespace

bool Attach(ModuleHandle loaderModule) {
    if (payloadModule) return true;

    const DetectedGame game = DetectGame();
    if (game == DetectedGame::Unknown) {
        ShowError(
            "Failed to detect supported GTA runtime.\n\n"
            "Supported games: GTA SA, GTA Vice City, GTA III.");
        return false;
    }

    const std::string payloadPath = PayloadPath(
        reinterpret_cast<HMODULE>(loaderModule), game);
    payloadModule = LoadLibraryA(payloadPath.c_str());
    if (payloadModule) return true;

    const DWORD errorCode = GetLastError();
    char message[768]{};
    std::snprintf(
        message,
        sizeof(message),
        "Failed to load XMenu payload.\n\nExpected file:\n%s\n\nError code: %lu",
        payloadPath.c_str(),
        static_cast<unsigned long>(errorCode));
    ShowError(message);
    return false;
}

void Detach() {
    if (!payloadModule) return;
    FreeLibrary(payloadModule);
    payloadModule = nullptr;
}

bool IsAttached() {
    return payloadModule != nullptr;
}

} // namespace XBase::Bootstrap