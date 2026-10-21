@echo off
setlocal EnableExtensions
cd /d "%~dp0" || exit /b 1

rem One Windows workflow: build.cmd -> out\game\Fable2Launcher.exe.
rem out\game is BOTH the disc extraction root and the installed game directory.
rem out\build holds only intermediate CMake/SDK compilation artifacts.
set "ROOT=%~dp0"
set "GAME=%ROOT%out\game"
set "BUILDDIR=%ROOT%out\build\win-amd64-release"
set "SDK=%ROOT%out\tooling\rexglue-sdk-0.10.0\win-amd64"
set "RUNTIME=%ROOT%out\tooling\runtime-sdk\win-amd64"
set "SDKSRC=%ROOT%thirdparty\rexglue-sdk"

if /i "%~1"=="clean" if "%~2"=="" goto clean
if not "%~1"=="" (
    echo Usage: build.cmd [clean] 1>&2
    exit /b 2
)

where dotnet >nul 2>nul || (
    echo Error: .NET 8 SDK is required to build the Windows launcher. 1>&2
    exit /b 1
)

rem Publish the launcher FIRST: on a fresh checkout it can extract the original
rem disc before native code generation needs default.xex. Do not touch existing
rem game data, saves, or settings when publishing.
echo [1/4] Publishing launcher to out\game ...
call "%ROOT%launcher\build-launcher.cmd" || exit /b 1

rem An extracted retail disc and both binaries always live under out\game.
rem No fallback to old source-root data/, and no separate Game Location setting.
if not exist "%GAME%\default.xex" goto extract
if not exist "%GAME%\data\dir.manifest" goto extract
goto content_ready

:extract
echo.
echo [2/4] Game content is missing from out\game.
echo Select your own Fable II disc ISO in the launcher window.
start "" /wait "%GAME%\Fable2Launcher.exe" --extract-for-build
if errorlevel 1 (
    echo Error: ISO extraction was cancelled or failed. 1>&2
    exit /b 1
)
if not exist "%GAME%\default.xex" (
    echo Error: out\game\default.xex is missing after extraction. 1>&2
    exit /b 1
)
if not exist "%GAME%\data\dir.manifest" (
    echo Error: out\game\data\dir.manifest is missing after extraction. 1>&2
    exit /b 1
)
:content_ready
echo [2/4] Game content found in out\game.

rem ReXGlue needs Clang, Ninja, CMake, Python and the pinned SDK. The project
rem downloads only the code-generation SDK; the native runtime is rebuilt from
rem its checked-in source submodule.
where clang++ >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\LLVM\bin\clang++.exe" set "PATH=C:\Program Files\LLVM\bin;%PATH%"
)
where clang++ >nul 2>nul || (
    echo Error: clang++ not found. Install LLVM or add it to PATH. 1>&2
    exit /b 1
)
where ninja >nul 2>nul
if errorlevel 1 (
    if exist "%USERPROFILE%\bin\ninja.exe" set "PATH=%USERPROFILE%\bin;%PATH%"
    for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_*") do (
        if exist "%%~fD\ninja.exe" set "PATH=%%~fD;%PATH%"
    )
)
where ninja >nul 2>nul || (
    echo Error: Ninja not found on PATH. 1>&2
    exit /b 1
)
for %%T in (cmake python git) do (
    where %%T >nul 2>nul || (
        echo Error: %%T not found on PATH. 1>&2
        exit /b 1
    )
)

if not exist "%SDK%\lib\cmake\rexglue\rexglueConfig.cmake" (
    echo Downloading pinned ReXGlue codegen SDK ...
    call "%ROOT%tools\setup_sdk.cmd" || exit /b 1
)
if not exist "%SDK%\bin\rexglue.exe" (
    echo Error: ReXGlue SDK is incomplete under "%SDK%". 1>&2
    exit /b 1
)
if not exist "%SDKSRC%\CMakeLists.txt" (
    git submodule update --init thirdparty/rexglue-sdk || exit /b 1
)
rem The tracked manifest points directly to out/game/default.xex.
rem This also survives ReXGlue regenerating its own CMake integration file.
rem First-time CMake configuration must see generated/default/sources.cmake
rem already on disk; otherwise rexglue_setup_target cannot add guest code to
rem the executable during configure, even though codegen runs later in Ninja.
if not exist "%ROOT%generated\default\sources.cmake" (
    echo Generating guest-code source list for first-time CMake setup ...
    "%SDK%\bin\rexglue.exe" codegen "%ROOT%fable_2_manifest.toml" || exit /b 1
    if not exist "%ROOT%generated\default\sources.cmake" (
        echo Error: ReXGlue did not produce generated\default\sources.cmake. 1>&2
        exit /b 1
    )
)
echo [3/4] Building matched source SDK ...
call "%ROOT%tools\build_runtime_sdk.cmd" "%SDK%" Release || exit /b 1

rem Use the same tracked manifest for code generation and CMake rebuilds.
echo [4/4] Configuring and building game ...
cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH="%RUNTIME%" -Drexglue_DIR="%RUNTIME%\lib\cmake\rexglue" -DREXGLUE_SDK_ROOT="%RUNTIME%" -DREXGLUE_SDK_SOURCE="%SDKSRC%" -DFABLE2_CODEGEN_TOOL="%SDK%\bin\rexglue.exe" || exit /b 1
cmake --build "%BUILDDIR%" --target fable_2 || exit /b 1
if not exist "%GAME%\fable_2.exe" (
    echo Error: out\game\fable_2.exe not produced by the build. 1>&2
    exit /b 1
)
if not exist "%GAME%\Fable2Launcher.exe" (
    echo Error: out\game\Fable2Launcher.exe not produced by the build. 1>&2
    exit /b 1
)
echo.
echo Build complete. Launch "%GAME%\Fable2Launcher.exe"
exit /b 0

:clean
rem Clean build intermediates only, never delete extracted content or saves.
if exist "%BUILDDIR%" rmdir /s /q "%BUILDDIR%"
if exist "%ROOT%out\build\runtime-sdk" rmdir /s /q "%ROOT%out\build\runtime-sdk"
echo Build intermediates removed. Game content and saves in out\game preserved.
exit /b 0
