@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0"

if /i "%~1"=="--no-pause" set "NO_PAUSE=1"

echo ==========================================
echo    XBase Project Generator
echo ==========================================

set "PREMAKE_EXE="
if exist "tools\premake5.exe" set "PREMAKE_EXE=tools\premake5.exe"
if not defined PREMAKE_EXE if exist "..\XMenu\tools\premake5.exe" set "PREMAKE_EXE=..\XMenu\tools\premake5.exe"
if not defined PREMAKE_EXE for /f "tokens=*" %%i in ('where premake5.exe 2^>nul') do if not defined PREMAKE_EXE set "PREMAKE_EXE=%%i"
if not defined PREMAKE_EXE for /f "tokens=*" %%i in ('where premake5 2^>nul') do if not defined PREMAKE_EXE set "PREMAKE_EXE=%%i"

if not defined PREMAKE_EXE (
    echo [Error] premake5 executable not found.
    goto fail
)

if "%PLUGIN_SDK_DIR%"=="" (
    if exist "..\plugin-sdk\" (
        set "PLUGIN_SDK_DIR=..\plugin-sdk"
        echo [Info] Auto-detected PLUGIN_SDK_DIR=!PLUGIN_SDK_DIR!
    ) else if exist "..\..\plugin-sdk\" (
        set "PLUGIN_SDK_DIR=..\..\plugin-sdk"
        echo [Info] Auto-detected PLUGIN_SDK_DIR=!PLUGIN_SDK_DIR!
    ) else (
        echo [Error] plugin-sdk not found. Three-version backends require PLUGIN_SDK_DIR.
        goto fail
    )
)

if not exist "%PLUGIN_SDK_DIR%" (
    echo [Error] Invalid plugin-sdk directory: %PLUGIN_SDK_DIR%
    goto fail
)

echo Generating Visual Studio 2022 project files...
"!PREMAKE_EXE!" vs2022
if errorlevel 1 (
    echo [Error] Project generation failed.
    goto fail
)

if exist "build\XBase.sln" (
    echo Project generation completed successfully.
    echo You can now open "build\XBase.sln" in Visual Studio.
) else (
    echo [Error] Failed to generate project files.
    goto fail
)

popd
if not defined NO_PAUSE pause
exit /b 0

:fail
popd
if not defined NO_PAUSE pause
exit /b 1
