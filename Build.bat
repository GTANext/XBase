@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0"

set "CONFIG=Release"
set "NO_PAUSE="
if not defined PLATFORM_TOOLSET set "PLATFORM_TOOLSET=v145"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="Debug" (
    set "CONFIG=Debug"
    shift
    goto parse_args
)
if /i "%~1"=="Release" (
    set "CONFIG=Release"
    shift
    goto parse_args
)
if /i "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
    shift
    goto parse_args
)
if /i "%~1"=="--toolset" goto parse_toolset
echo [Error] Unknown argument: %~1
echo Usage: Build.bat [Debug^|Release] [--toolset v145] [--no-pause]
goto fail

:parse_toolset
shift
if "%~1"=="" (
    echo [Error] --toolset requires a value, e.g. v145 / v143
    goto fail
)
set "TOOLSET_VALUE=%~1"
if /i "%TOOLSET_VALUE:~0,1%" NEQ "v" (
    echo [Error] Invalid platform toolset: %TOOLSET_VALUE%
    echo [Error] Expected a value such as v145 or v143.
    goto fail
)
for /f "delims=0123456789" %%C in ("%TOOLSET_VALUE:~1%") do (
    echo [Error] Invalid platform toolset: %TOOLSET_VALUE%
    echo [Error] Expected a value such as v145 or v143.
    goto fail
)
if "%TOOLSET_VALUE:~1%"=="" (
    echo [Error] Invalid platform toolset: %TOOLSET_VALUE%
    goto fail
)
set "PLATFORM_TOOLSET=%TOOLSET_VALUE%"
shift
goto parse_args

:args_done
if not defined PLATFORM_TOOLSET (
    echo [Error] PlatformToolset is empty.
    goto fail
)

echo ==========================================
echo    XBase Builder
echo ==========================================
echo Configuration: %CONFIG%
echo Platform: Win32
echo PlatformToolset: %PLATFORM_TOOLSET%
echo.

call :find_premake
if not defined PREMAKE_EXE (
    echo [Error] premake5 executable not found.
    goto fail
)

call :detect_plugin_sdk
if not defined PLUGIN_SDK_DIR (
    echo [Error] PLUGIN_SDK_DIR is required for all XBase targets.
    goto fail
)

if not exist "build" mkdir "build"

echo Generating Visual Studio 2022 project files...
"!PREMAKE_EXE!" vs2022
if errorlevel 1 (
    echo [Error] Project generation failed.
    goto fail
)

if not exist "build\XBase.sln" (
    echo [Error] build\XBase.sln not found.
    goto fail
)

call :find_msbuild
if not defined MSBUILD_EXE (
    echo [Error] MSBuild.exe not found.
    goto fail
)

echo.
echo Using premake: !PREMAKE_EXE!
echo Using MSBuild: !MSBUILD_EXE!

"!MSBUILD_EXE!" "build\XBase.sln" /m /t:XBaseSA /p:Configuration=%CONFIG% /p:Platform=Win32 /p:PlatformToolset=%PLATFORM_TOOLSET% /verbosity:minimal
if errorlevel 1 (
    echo.
    echo [Error] XBaseSA build failed.
    goto fail
)

"!MSBUILD_EXE!" "build\XBase.sln" /m /t:XBaseVC /p:Configuration=%CONFIG% /p:Platform=Win32 /p:PlatformToolset=%PLATFORM_TOOLSET% /verbosity:minimal
if errorlevel 1 (
    echo.
    echo [Error] XBaseVC build failed.
    goto fail
)

"!MSBUILD_EXE!" "build\XBase.sln" /m /t:XBaseIII /p:Configuration=%CONFIG% /p:Platform=Win32 /p:PlatformToolset=%PLATFORM_TOOLSET% /verbosity:minimal
if errorlevel 1 (
    echo.
    echo [Error] XBaseIII build failed.
    goto fail
)

for %%T in (XBaseSA XBaseVC XBaseIII) do (
    if not exist "build\bin\%CONFIG%\%%T.lib" (
        echo [Error] build\bin\%CONFIG%\%%T.lib was not produced.
        goto fail
    )
)

echo.
echo Build completed successfully.
echo Outputs: XBaseSA.lib, XBaseVC.lib, XBaseIII.lib
goto success

:find_premake
set "PREMAKE_EXE="
if exist "tools\premake5.exe" set "PREMAKE_EXE=%~dp0tools\premake5.exe" & goto :eof
if exist "..\XMenu\tools\premake5.exe" set "PREMAKE_EXE=%~dp0..\XMenu\tools\premake5.exe" & goto :eof
for /f "tokens=*" %%i in ('where premake5.exe 2^>nul') do if not defined PREMAKE_EXE set "PREMAKE_EXE=%%i"
if not defined PREMAKE_EXE (
    for /f "tokens=*" %%i in ('where premake5 2^>nul') do if not defined PREMAKE_EXE set "PREMAKE_EXE=%%i"
)
exit /b 0

:detect_plugin_sdk
if "%PLUGIN_SDK_DIR%"=="" (
    if exist "..\plugin-sdk\" (
        set "PLUGIN_SDK_DIR=%~dp0..\plugin-sdk"
        echo [Info] Auto-detected PLUGIN_SDK_DIR=!PLUGIN_SDK_DIR!
    ) else if exist "..\..\plugin-sdk\" (
        set "PLUGIN_SDK_DIR=%~dp0..\..\plugin-sdk"
        echo [Info] Auto-detected PLUGIN_SDK_DIR=!PLUGIN_SDK_DIR!
    ) else (
        echo [Error] plugin-sdk not found. Three-version backends require PLUGIN_SDK_DIR.
    )
)
if not "%PLUGIN_SDK_DIR%"=="" (
    if exist "%PLUGIN_SDK_DIR%" (
        echo [Info] Using plugin-sdk directory: %PLUGIN_SDK_DIR%
    ) else (
        echo [Warning] Ignoring invalid PLUGIN_SDK_DIR: %PLUGIN_SDK_DIR%
        set "PLUGIN_SDK_DIR="
        echo [Error] Three-version backends cannot be generated without plugin-sdk.
    )
)
exit /b 0

:find_msbuild
if defined MSBUILD_EXE if exist "!MSBUILD_EXE!" exit /b 0
set "MSBUILD_EXE="
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD_EXE (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
            if not defined MSBUILD_EXE set "MSBUILD_EXE=%%i"
        )
    )
)
if not defined MSBUILD_EXE (
    for %%i in (
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    ) do if exist %%~si if not defined MSBUILD_EXE set "MSBUILD_EXE=%%~si"
)
if not defined MSBUILD_EXE (
    for /f "tokens=*" %%i in ('where MSBuild.exe 2^>nul') do if not defined MSBUILD_EXE set "MSBUILD_EXE=%%i"
)
exit /b 0

:success
popd
if not defined NO_PAUSE pause
exit /b 0

:fail
popd
if not defined NO_PAUSE pause
exit /b 1
