@echo off
setlocal enabledelayedexpansion

rem ===========================================================================
rem  Veltrix Studio core - Windows build script (MSVC + Ninja + CMake presets)
rem
rem  Usage:
rem      scripts\build-windows.bat              (release + tests)
rem      scripts\build-windows.bat debug       (debug + tests)
rem
rem  Auto-locates Visual Studio via vswhere. Set VELTRIX_VS_PATH to point
rem  at a VS installation that vswhere does not list (e.g. Build Tools
rem  on a non-standard drive). No machine-specific paths are hardcoded.
rem ===========================================================================

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=release"

set "PRESET="
if /i "%CONFIG%"=="debug"  set "PRESET=win-msvc-debug"
if /i "%CONFIG%"=="release" set "PRESET=win-msvc-release"
if "%PRESET%"=="" (
    echo ERROR: unknown configuration "%CONFIG%" ^(expected: debug or release^)
    exit /b 2
)

set "VSPATH="
if not "%VELTRIX_VS_PATH%"=="" (
    set "VSPATH=%VELTRIX_VS_PATH%"
    echo Using VELTRIX_VS_PATH override: !VSPATH!
) else (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
    )
)

if "!VSPATH!"=="" (
    echo ERROR: Visual Studio ^(C++ tools^) not found.
    echo        Install "Visual Studio Build Tools" with the C++ workload, or
    echo        set VELTRIX_VS_PATH to the VS installation directory.
    exit /b 1
)

call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars64.bat failed to initialize the MSVC environment.
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not found on PATH after vcvars64. Install the
    echo        "C++ CMake tools for Windows" VS component or CMake ^>= 3.24.
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo ERROR: ninja not found on PATH after vcvars64. Install the
    echo        "C++ CMake tools for Windows" VS component or Ninja.
    exit /b 1
)

echo.
echo === veltrix core: configure (%PRESET%) ===
cmake --preset %PRESET%
if errorlevel 1 exit /b 1

echo.
echo === veltrix core: build ===
cmake --build --preset %PRESET%
if errorlevel 1 exit /b 1

echo.
echo === veltrix core: tests ===
ctest --preset %PRESET% --output-on-failure
exit /b %ERRORLEVEL%
