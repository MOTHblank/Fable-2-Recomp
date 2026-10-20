@echo off
rem Build a dual-backend Vulkan-enabled SDK from the SAME pinned, prepared
rem thirdparty\rexglue-sdk source used by tools\build_runtime_sdk.cmd.
rem Output is independently snapshotted into out\tooling\vulkan-sdk so that
rem a subsequent D3D12 rebuild cannot silently replace the Vulkan pair.
setlocal
set "REPO=%~dp0.."
set "SOURCE=%REPO%\thirdparty\rexglue-sdk"
set "WRAPPER=%REPO%\tools\vulkan_sdk"
set "BUILD=%REPO%\out\build\vulkan-sdk"
set "OUTPUT=%REPO%\out\tooling\vulkan-sdk\win-amd64"
call "%~dp0setup_sdk_src.cmd" || exit /b 1
where clang++ >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\LLVM\bin\clang++.exe" set "PATH=C:\Program Files\LLVM\bin;%PATH%"
)
where clang++ >nul 2>nul || (echo Error: LLVM clang++ not found. 1>&2 & exit /b 1)
where ninja >nul 2>nul
if errorlevel 1 if exist "%USERPROFILE%\bin\ninja.exe" set "PATH=%USERPROFILE%\bin;%PATH%"
where ninja >nul 2>nul || (echo Error: Ninja not found on PATH. 1>&2 & exit /b 1)
cmake -S "%WRAPPER%" -B "%BUILD%" -G "Ninja Multi-Config" ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 ^
  -DCMAKE_CXX_STANDARD=23 -DCMAKE_CONFIGURATION_TYPES=Release ^
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF ^
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_SDK_SOURCE="%SOURCE%" || exit /b 1
cmake --build "%BUILD%" --config Release --target rexgpu-xenos rexruntime || exit /b 1
python "%~dp0stage_vulkan_sdk.py" "%SOURCE%" "%BUILD%" "%OUTPUT%" || exit /b 1
echo Vulkan pair staged at %OUTPUT%
echo Rebuild the Release game to publish it under renderers\vulkan.
exit /b 0
