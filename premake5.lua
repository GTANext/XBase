workspace "XBase"
    configurations { "Debug", "Release" }
    platforms { "Win32" }
    architecture "x86"
    language "C++"
    cppdialect "C++20"
    characterset "MBCS"
    staticruntime "On"
    toolset "msc"
    buildoptions { "/utf-8", "/FS" }
    location "build"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"

    local pluginSdkDir = os.getenv("PLUGIN_SDK_DIR")
    if pluginSdkDir == nil or pluginSdkDir == "" then
        if os.isdir("../plugin-sdk") then
            pluginSdkDir = "../plugin-sdk"
        elseif os.isdir("../../plugin-sdk") then
            pluginSdkDir = "../../plugin-sdk"
        end
    end

    local hasPluginSdk = false
    if pluginSdkDir ~= nil and pluginSdkDir ~= "" and os.isdir(pluginSdkDir) then
        hasPluginSdk = true
    end

    if not hasPluginSdk then
        error("plugin-sdk is required to build XBaseSA, XBaseVC, and XBaseIII")
    end

    local hasKiero = os.isdir("include/kiero")

    defines {
        "_CRT_SECURE_NO_WARNINGS",
        "_CRT_NON_CONFORMING_SWPRINTFS",
    }

local function add_entry_target(name, sources)
    project(name)
        kind "StaticLib"
        targetname(name)
        files(sources)

        filter "configurations:Debug"
            defines { "DEBUG" }
            optimize "Off"
            symbols "On"
        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "Speed"
            symbols "Off"
        filter {}
end

add_entry_target("XBaseBootstrap", {
    "src/Bootstrap.h",
    "src/Bootstrap.cpp",
    "src/BootstrapEntry.cpp"
})

add_entry_target("XBasePayloadEntry", {
    "src/PayloadEntry.cpp"
})

local function add_sa_settings()
    if hasPluginSdk then
        defines { "XBASE_WITH_PLUGIN_SDK", "XBASE_BACKEND_SA", "GTASA", "_GTA_", "RW", "IS_PLATFORM_WIN" }
        includedirs {
            "include",
            "src/backends",
            path.join(pluginSdkDir, "plugin_sa"),
            path.join(pluginSdkDir, "plugin_sa", "game_sa"),
            path.join(pluginSdkDir, "plugin_sa", "game_sa", "enums"),
            path.join(pluginSdkDir, "plugin_sa", "game_sa", "rw"),
            path.join(pluginSdkDir, "shared"),
            path.join(pluginSdkDir, "shared", "game"),
            path.join(pluginSdkDir, "shared", "dxsdk")
        }
    end
    if hasKiero then
        defines { "XBASE_WITH_KIERO" }
        includedirs { "include/imgui", "include/kiero" }
    end
end

local function add_portable_player_target(name, sdkName, gameName, gameDefine, backendDefine)
    project(name)
        kind "StaticLib"
        targetname(name)
        files {
            "include/XBase/**.h",
            "src/backends/BulletAssistBackend.h",
            "src/backends/BulletAssistBackend_" .. string.lower(sdkName) .. ".cpp",
            "src/backends/PlayerBackend.h",
            "src/backends/PlayerBackend_" .. string.lower(sdkName) .. ".cpp",
            "src/backends/PedBackend.h",
            "src/backends/PedBackend_" .. string.lower(sdkName) .. ".cpp",
            "src/backends/VehicleBackend.h",
            "src/backends/VehicleBackend_" .. string.lower(sdkName) .. ".cpp",
            "src/controllers/Capabilities.cpp",
            "src/controllers/BulletAssist.cpp",
            "src/controllers/Config.cpp",
            "src/controllers/CoreStub.cpp",
            "src/controllers/I18n.cpp",
            "src/controllers/Json.cpp",
            "src/controllers/Log.cpp",
            "src/controllers/Hooks.cpp",
            "src/controllers/Input.cpp",
            "src/controllers/Host.cpp",
            "src/controllers/Platform.cpp",
            "src/controllers/Runtime.cpp",
            "src/controllers/RenderFonts.cpp",
            "src/controllers/RenderFonts.h",
            "src/controllers/Theme.cpp",
            "src/controllers/UI.cpp",
            "src/controllers/PlayerPortable.cpp",
            "src/controllers/PedPortable.cpp",
            "src/controllers/PortableStubs.cpp",
            "src/controllers/VehiclePortable.cpp",
            "include/imgui/imgui.cpp",
            "include/imgui/imgui_draw.cpp",
            "include/imgui/imgui_tables.cpp",
            "include/imgui/imgui_widgets.cpp",
            "include/imgui/imgui_impl_win32.cpp",
            "include/imgui/imgui_impl_dx9.cpp",
            "include/kiero/kiero.cpp",
            "include/kiero/minhook/buffer.c",
            "include/kiero/minhook/hook.c",
            "include/kiero/minhook/trampoline.c",
            "include/kiero/minhook/hde/hde32.c"
        }
        includedirs {
            "include",
            "src/backends",
            "include/imgui",
            "include/kiero",
            path.join(pluginSdkDir, "plugin_" .. sdkName),
            path.join(pluginSdkDir, "plugin_" .. sdkName, "game_" .. gameName),
            path.join(pluginSdkDir, "plugin_" .. sdkName, "game_" .. gameName, "enums"),
            path.join(pluginSdkDir, "plugin_" .. sdkName, "game_" .. gameName, "rw"),
            path.join(pluginSdkDir, "shared"),
            path.join(pluginSdkDir, "shared", "game"),
            path.join(pluginSdkDir, "shared", "dxsdk")
        }
        defines {
            "XBASE_WITH_PLUGIN_SDK",
            "XBASE_WITH_KIERO",
            backendDefine,
            gameDefine,
            "_GTA_",
            "RW",
            "IS_PLATFORM_WIN"
        }

        filter "configurations:Debug"
            defines { "DEBUG" }
            optimize "Off"
            symbols "On"
        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "Speed"
            symbols "Off"
        filter {}
end

project "XBaseSA"
    kind "StaticLib"
    targetname "XBaseSA"
    files {
        "include/XBase/**.h",
        "src/**.h",
        "src/controllers/*.cpp",
        "src/controllers/Camera.cpp",
        "src/controllers/Cheats.cpp",
        "src/controllers/VehicleEffects.cpp",
        "src/backends/BulletAssistBackend_sa.cpp",
        "include/imgui/imgui.cpp",
        "include/imgui/imgui_draw.cpp",
        "include/imgui/imgui_tables.cpp",
        "include/imgui/imgui_widgets.cpp",
        "include/imgui/imgui_impl_win32.cpp",
        "include/imgui/imgui_impl_dx9.cpp",
        "include/kiero/kiero.cpp",
        "include/kiero/minhook/buffer.c",
        "include/kiero/minhook/hook.c",
        "include/kiero/minhook/trampoline.c",
        "include/kiero/minhook/hde/hde32.c"
    }
    removefiles {
        "src/main.cpp",
        "src/controllers/CoreStub.cpp",
        "src/controllers/PlayerPortable.cpp",
        "src/controllers/PedPortable.cpp",
        "src/controllers/PortableStubs.cpp",
        "src/controllers/VehiclePortable.cpp",
        "src/backends/PlayerBackend_vc.cpp",
        "src/backends/PlayerBackend_iii.cpp",
        "src/backends/VehicleBackend_vc.cpp",
        "src/backends/VehicleBackend_iii.cpp"
    }
    add_sa_settings()

    filter "configurations:Debug"
        defines { "DEBUG" }
        optimize "Off"
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Speed"
        symbols "Off"
    filter {}

add_portable_player_target("XBaseVC", "vc", "vc", "GTAVC", "XBASE_BACKEND_VC")
add_portable_player_target("XBaseIII", "III", "III", "GTA3", "XBASE_BACKEND_III")
