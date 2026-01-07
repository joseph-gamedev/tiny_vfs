@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
cd /d "%ROOT%"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake not found. Install CMake and retry.
    exit /b 1
)

set "GEN="
if defined TINYVFS_CMAKE_GENERATOR set "GEN=%TINYVFS_CMAKE_GENERATOR%"

if not defined GEN (
    set "VSWHERE="
    set "VSYEAR="
    for %%p in (
        "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
        "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    ) do if exist "%%~p" set "VSWHERE=%%~p"

    if defined VSWHERE (
        for /f "usebackq delims=" %%i in (
            `"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property catalog_productLineVersion`
        ) do set "VSYEAR=%%i"
        if "!VSYEAR!"=="2022" set "GEN=Visual Studio 17 2022"
        if "!VSYEAR!"=="2019" set "GEN=Visual Studio 16 2019"
    )
)

if not defined GEN (
    echo MSVC not found. Install Visual Studio with C++ or set TINYVFS_CMAKE_GENERATOR.
    exit /b 1
)

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"

if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

echo Using generator: !GEN!
echo Configuring...

echo !GEN! | findstr /i "Visual Studio" >nul
if not errorlevel 1 (
    cmake -S . -B "!BUILD_DIR!" -G "!GEN!" -A x64
) else (
    cmake -S . -B "!BUILD_DIR!" -G "!GEN!" -DCMAKE_BUILD_TYPE=!BUILD_TYPE!
)
if errorlevel 1 exit /b 1

echo Building...
cmake --build "!BUILD_DIR!" --config !BUILD_TYPE!
if errorlevel 1 exit /b 1

echo Running tests...
ctest --test-dir "!BUILD_DIR!" -C !BUILD_TYPE!
if errorlevel 1 exit /b 1

if exist examples\assets\hello.txt (
    if exist "!BUILD_DIR!\!BUILD_TYPE!\tiny_vfs_example.exe" (
        echo Running example...
        "!BUILD_DIR!\!BUILD_TYPE!\tiny_vfs_example.exe"
    ) else if exist "!BUILD_DIR!\tiny_vfs_example.exe" (
        echo Running example...
        "!BUILD_DIR!\tiny_vfs_example.exe"
    )
)

echo Done.
