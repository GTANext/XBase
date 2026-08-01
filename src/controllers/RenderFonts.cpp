#include "RenderFonts.h"

#include <XBase/Log.h>

#include <Windows.h>
#include "imgui/imgui.h"

#include <array>
#include <string>

namespace XBase::Detail::RenderFonts {
namespace {

struct FontCandidate {
    const char* fileName;
    const char* displayName;
};

constexpr std::array<FontCandidate, 16> FontCandidates{{
    {"msyh.ttc", "Microsoft YaHei"},
    {"msyh.ttf", "Microsoft YaHei"},
    {"msyhbd.ttc", "Microsoft YaHei Bold"},
    {"simhei.ttf", "SimHei"},
    {"simsun.ttc", "SimSun"},
    {"simfang.ttf", "FangSong"},
    {"simkai.ttf", "KaiTi"},
    {"Deng.ttf", "DengXian"},
    {"Dengb.ttf", "DengXian Bold"},
    {"STZHONGS.TTF", "STZhongsong"},
    {"STSONG.TTF", "STSong"},
    {"STXIHEI.TTF", "STXihei"},
    {"NotoSansCJK-Regular.ttc", "Noto Sans CJK"},
    {"SourceHanSansCN-Regular.otf", "Source Han Sans CN"},
    {"SourceHanSerifCN-Regular.otf", "Source Han Serif CN"},
    {"arialuni.ttf", "Arial Unicode MS"},
}};

const ImWchar GlyphRanges[] = {
    0x0020, 0x00FF,
    0x0400, 0x052F,
    0x3000, 0x30FF,
    0x31F0, 0x31FF,
    0x3400, 0x4DBF,
    0x4E00, 0x9FFF,
    0xF900, 0xFAFF,
    0xFF00, 0xFFEF,
    0,
};

bool IsFile(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

} // namespace

bool LoadDefault() {
    if (!ImGui::GetCurrentContext()) return false;

    char windowsPath[MAX_PATH]{};
    const UINT pathSize = GetWindowsDirectoryA(windowsPath, MAX_PATH);
    if (pathSize == 0 || pathSize >= MAX_PATH) {
        Log::Warn("RenderFonts: Windows font directory unavailable");
        return false;
    }

    const std::string fontsDirectory = std::string(windowsPath) + "\\Fonts\\";
    ImGuiIO& io = ImGui::GetIO();
    for (const FontCandidate& candidate : FontCandidates) {
        const std::string path = fontsDirectory + candidate.fileName;
        if (!IsFile(path)) continue;

        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), 18.0f, nullptr, GlyphRanges);
        if (!font) {
            Log::Warn("RenderFonts: failed to load " + path);
            continue;
        }

        io.FontDefault = font;
        Log::Info(std::string("RenderFonts: loaded ") + candidate.displayName);
        return true;
    }

    Log::Warn("RenderFonts: no multilingual font candidate found; using ImGui default");
    return false;
}

} // namespace XBase::Detail::RenderFonts