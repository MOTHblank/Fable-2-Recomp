@echo off
rem Build and install a matched Windows x64 ReXGlue SDK from the pinned source.
rem Usage: build_runtime_sdk.cmd [Release|Debug]
rem A legacy second argument is accepted so older calls of
rem   build_runtime_sdk.cmd <official-sdk-path> Release
rem continue to select Release while ignoring the obsolete package path.
rem Run in a VS x64 developer shell with LLVM 20+, Ninja, CMake and Python.
setlocal
set "ROOT=%~dp0.."
set "SOURCE=%ROOT%\thirdparty\rexglue-sdk"
set "BUILD=%ROOT%\out\build\runtime-sdk"
set "STAGED=%ROOT%\out\tooling\runtime-sdk\win-amd64"

set "CONFIGURATION=%~1"
if not "%~2"=="" set "CONFIGURATION=%~2"
if not defined CONFIGURATION set "CONFIGURATION=Release"

if /i "%CONFIGURATION%"=="Debug" (
  set "CONFIGURATION=Debug"
  set "BUILD=%ROOT%\out\build\runtime-sdk-debug"
  set "STAGED=%ROOT%\out\tooling\runtime-sdk-debug\win-amd64"
) else if /i "%CONFIGURATION%"=="Release" (
  set "CONFIGURATION=Release"
) else (
  echo Error: runtime configuration must be Debug or Release. 1>&2
  exit /b 1
)

git -C "%ROOT%" submodule sync -- thirdparty/rexglue-sdk || exit /b 1
if not exist "%SOURCE%\CMakeLists.txt" (
  git -C "%ROOT%" submodule update --init thirdparty/rexglue-sdk || exit /b 1
)

rem The checked-out SDK can remain at an older commit after git pull. Synchronize
rem clean checkouts with the gitlink, but never overwrite locally patched SDK files.
set "EXPECTED_SDK="
set "CURRENT_SDK="
for /f %%H in ('git -C "%ROOT%" rev-parse ":thirdparty/rexglue-sdk" 2^>nul') do set "EXPECTED_SDK=%%H"
for /f %%H in ('git -C "%SOURCE%" rev-parse HEAD 2^>nul') do set "CURRENT_SDK=%%H"
if not defined EXPECTED_SDK (
  echo Error: cannot determine the tracked ReXGlue SDK revision. 1>&2
  exit /b 1
)
if /i not "%CURRENT_SDK%"=="%EXPECTED_SDK%" (
  git -C "%SOURCE%" diff --quiet || (
    echo Error: ReXGlue SDK has local edits. Preserve or restore them before changing revisions. 1>&2
    exit /b 1
  )
  git -C "%SOURCE%" diff --cached --quiet || (
    echo Error: ReXGlue SDK has staged local edits. Preserve them before updating. 1>&2
    exit /b 1
  )
  echo Updating SDK checkout to tracked revision %EXPECTED_SDK% ...
  git -C "%ROOT%" submodule update --init thirdparty/rexglue-sdk || exit /b 1
)

python "%~dp0prepare_runtime_sdk.py" "%SOURCE%" || exit /b 1

rem -g always emits runtime/plugin PDBs. Release remains optimized while
rem retaining symbols for crash analysis.
cmake -S "%SOURCE%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=%CONFIGURATION% -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_C_FLAGS="-march=x86-64-v2 -g" -DCMAKE_CXX_FLAGS="-march=x86-64-v2 -g" ^
  -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=OFF ^
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF ^
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_REGISTER_PACKAGE=OFF || exit /b 1

rem Build the complete default SDK graph. In Release this includes rexglue.exe,
rem whose runtime imports are exported by the pinned SDK fork.
cmake --build "%BUILD%" || exit /b 1

rem The staging directory is generated tooling, never game/user data. Recreate
rem it so an older official/hybrid package cannot leave stale files behind.
if exist "%STAGED%" rmdir /s /q "%STAGED%"
cmake --install "%BUILD%" --prefix "%STAGED%" --config %CONFIGURATION% || exit /b 1

python "%~dp0stage_renderer_sdk.py" "%SOURCE%" "%BUILD%" "%STAGED%" --configuration %CONFIGURATION% || exit /b 1
exit /b 0
