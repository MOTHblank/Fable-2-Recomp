@echo off
rem Prepare the ONE SDK source tree used by both D3D12 and Vulkan builds.
rem The superproject gitlink pins thirdparty\rexglue-sdk. Never clone a second
rem untracked SDK or apply the obsolete Vulkan diagnostic patch.
setlocal
set "REPO=%~dp0.."
set "SOURCE=%REPO%\thirdparty\rexglue-sdk"
where git >nul 2>nul || (echo Error: Git not found on PATH. 1>&2 & exit /b 1)
where python >nul 2>nul || (echo Error: Python not found on PATH. 1>&2 & exit /b 1)
git -C "%REPO%" submodule update --init thirdparty/rexglue-sdk || exit /b 1
if not exist "%SOURCE%\CMakeLists.txt" (
    echo Error: pinned SDK source missing at %SOURCE%. 1>&2
    exit /b 1
)
python "%~dp0prepare_runtime_sdk.py" "%SOURCE%" || exit /b 1
echo Prepared pinned renderer SDK source: %SOURCE%
exit /b 0
