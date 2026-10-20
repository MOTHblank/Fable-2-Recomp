@echo off
rem Fable 2 Release launcher. Each invocation uses its own verified runtime
rem and GPU DLLs; no in-place DLL swapping and no cross-process races.
rem Usage: fable2.cmd [d3d12 ^| vulkan]
setlocal
cd /d "%~dp0"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=d3d12"
if /i "%MODE%"=="prebuilt" (
    echo Error: prebuilt mode is disabled: the current game links against 1>&2
    echo matched source SDK headers and cannot verify prebuilt compatibility. 1>&2
    exit /b 1
)
if /i not "%MODE%"=="d3d12" if /i not "%MODE%"=="vulkan" (
    echo Usage: fable2.cmd [d3d12 ^| vulkan] 1>&2
    exit /b 1
)
if not exist "launch_renderer.ps1" (
    echo Error: launch_renderer.ps1 missing; rebuild the Release game. 1>&2
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch_renderer.ps1" -Backend "%MODE%"
exit /b %ERRORLEVEL%
