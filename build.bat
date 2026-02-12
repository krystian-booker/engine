@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: build.bat - One-step build script for the engine
::
:: Usage:
::   build.bat              Configure + build (Debug)
::   build.bat release      Configure + build (Release)
::   build.bat clean        Delete build dir and rebuild (Debug)
::   build.bat tests        Build with ENGINE_BUILD_TESTS=ON (Debug)
::   build.bat tests release   Build tests in Release
::   build.bat clean release   Clean rebuild in Release
:: ============================================================================

set "BUILD_TYPE=Debug"
set "DO_CLEAN=0"
set "BUILD_TESTS=0"

:: Parse arguments
for %%A in (%*) do (
    if /i "%%A"=="release" set "BUILD_TYPE=Release"
    if /i "%%A"=="clean"   set "DO_CLEAN=1"
    if /i "%%A"=="tests"   set "BUILD_TESTS=1"
)

set "PRESET=Qt-%BUILD_TYPE%"
set "BUILD_DIR=%~dp0out\build\%PRESET%"

echo.
echo === Engine Build ===
echo   Preset:  %PRESET%
echo   Clean:   %DO_CLEAN%
echo   Tests:   %BUILD_TESTS%
echo.

:: ---------------------------------------------------------------------------
:: 1. Find Visual Studio via vswhere
:: ---------------------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Is Visual Studio installed?
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo ERROR: No Visual Studio installation with C++ tools found.
    exit /b 1
)

echo   VS:      %VS_PATH%

:: ---------------------------------------------------------------------------
:: 2. Set up MSVC developer environment
:: ---------------------------------------------------------------------------
set "VSDEVCMD=%VS_PATH%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo ERROR: VsDevCmd.bat not found at %VSDEVCMD%
    exit /b 1
)

call "%VSDEVCMD%" -arch=amd64 -no_logo >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize VS developer environment.
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: 3. Add Ninja to PATH (bundled with VS)
:: ---------------------------------------------------------------------------
set "NINJA_DIR=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
if exist "%NINJA_DIR%\ninja.exe" (
    set "PATH=%NINJA_DIR%;%PATH%"
) else (
    where ninja >nul 2>&1
    if errorlevel 1 (
        echo WARNING: Ninja not found in VS bundle or PATH. CMake may fail.
    )
)

:: ---------------------------------------------------------------------------
:: 4. Clean if requested
:: ---------------------------------------------------------------------------
if "%DO_CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo   Removing %BUILD_DIR%...
        rmdir /s /q "%BUILD_DIR%"
    )
)

:: ---------------------------------------------------------------------------
:: 5. Configure
:: ---------------------------------------------------------------------------
set "CMAKE_ARGS=--preset %PRESET%"
if "%BUILD_TESTS%"=="1" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DENGINE_BUILD_TESTS=ON"
)

echo.
echo --- Configure ---
cmake %CMAKE_ARGS%
if errorlevel 1 (
    echo.
    echo ERROR: CMake configure failed.
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: 6. Build
:: ---------------------------------------------------------------------------
echo.
echo --- Build ---
cmake --build "%BUILD_DIR%"
if errorlevel 1 (
    echo.
    echo ERROR: Build failed.
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: 7. Run tests if requested
:: ---------------------------------------------------------------------------
if "%BUILD_TESTS%"=="1" (
    echo.
    echo --- Tests ---
    ctest --preset %PRESET%
    if errorlevel 1 (
        echo.
        echo ERROR: Tests failed.
        exit /b 1
    )
)

echo.
echo === Build complete ===
echo   Output: %BUILD_DIR%\bin\
echo.
