@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Fable 2 Recompiled - Install Build Requirements

rem Installs the Windows build requirements for MOTHblank/Fable-2-Recomp.
rem Run normally (auto-elevates) or use --verify for a read-only check.
rem Requires Windows 10/11 x64, internet access, and WinGet / App Installer.
rem Does NOT download game files, clone the repo, or build the project.

rem Make freshly installed tools discoverable in this process when possible.
set "PATH=%ProgramFiles%\LLVM\bin;%ProgramFiles%\CMake\bin;%ProgramFiles%\Git\cmd;%ProgramFiles%\dotnet;%LOCALAPPDATA%\Microsoft\WinGet\Links;%USERPROFILE%\bin;%PATH%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSSETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe"

if /i "%~1"=="--verify" goto :VerifyOnly
fltmc >nul 2>&1
if errorlevel 1 goto :Elevate

echo ============================================================
echo   Fable 2 Recompiled - Windows Build Requirements
echo ============================================================
echo.
echo This will install missing development tools using WinGet.
echo Visual Studio Build Tools can require several GB of downloads.
echo No game files will be downloaded or modified.
echo.

where winget.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] WinGet was not found.
    echo Install or update App Installer from the Microsoft Store,
    echo then open a new Command Prompt and run this file again.
    goto :Failed
)

set "FAILED=0"
call :EnsureTool "Git" "git.exe" "Git.Git"
call :EnsureCMake
call :EnsureTool "Ninja" "ninja.exe" "Ninja-build.Ninja"
call :EnsureLLVM
call :EnsurePython
call :EnsureDotNet8
call :EnsureVisualStudio

echo.
echo ============================================================
echo   Installation finished - checking the environment
echo ============================================================
call :Verify
if %FAILED% GTR 0 (
    echo.
    echo [WARNING] %FAILED% installation step^(s^) reported errors.
    echo Check the messages above. Retry after fixing the cause.
) else (
    echo.
    echo All install commands that were needed returned successfully.
)
echo.
echo IMPORTANT: Close and reopen Command Prompt after installation.
echo Use "x64 Native Tools Command Prompt for VS" to build.
echo After extracting your own supported Fable II GOTY game files:
echo   git clone --recurse-submodules https://github.com/MOTHblank/Fable-2-Recomp.git
echo   cd Fable-2-Recomp
echo   build.cmd -release fable_2
echo   build.cmd launcher
echo.
echo The build.cmd script obtains the ReXGlue SDK separately.
echo.
pause
if %FAILED% GTR 0 exit /b 1
exit /b 0

:EnsureTool
where "%~2" >nul 2>&1
if not errorlevel 1 (
    echo [OK] %~1 is on PATH.
    exit /b 0
)
echo.
echo [INSTALL] %~1 (%~3)
call :InstallPackage "%~3"
exit /b 0

rem CMake 3.25+ required by the repository's CMakePresets.json.
:EnsureCMake
call :CheckCMake
if not errorlevel 1 (
    echo [OK] CMake 3.25 or newer is available.
    exit /b 0
)
echo.
echo [INSTALL] CMake 3.25+ via WinGet
where cmake.exe >nul 2>&1
if errorlevel 1 (
    call :InstallPackage "Kitware.CMake"
) else (
    call :UpgradePackage "Kitware.CMake"
)
exit /b 0

rem The native source SDK requires LLVM/Clang 20+ and LLD.
:EnsureLLVM
call :CheckLLVM
if not errorlevel 1 (
    echo [OK] LLVM / Clang 20+ and LLD are available.
    exit /b 0
)
echo.
echo [INSTALL] LLVM / Clang 20+ / LLD via WinGet
where clang++.exe >nul 2>&1
if errorlevel 1 (
    call :InstallPackage "LLVM.LLVM"
) else (
    call :UpgradePackage "LLVM.LLVM"
)
exit /b 0

:EnsurePython
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
if not errorlevel 1 (
    echo [OK] Python 3.10 or newer is available.
    exit /b 0
)
echo.
echo [INSTALL] Python 3.13
call :InstallPackage "Python.Python.3.13"
exit /b 0

:EnsureDotNet8
set "HAVE_DOTNET8="
for /f "delims=" %%V in ('dotnet --list-sdks 2^>nul ^| findstr /R /B "8\."') do set "HAVE_DOTNET8=1"
if defined HAVE_DOTNET8 (
    echo [OK] .NET 8 SDK is installed.
    exit /b 0
)
echo.
echo [INSTALL] .NET 8 SDK (required for launcher)
call :InstallPackage "Microsoft.DotNet.SDK.8"
exit /b 0

:EnsureVisualStudio
set "VS_READY="
set "VS_EXISTING="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest`) do set "VS_READY=%%I"
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -products * -property installationPath -latest`) do set "VS_EXISTING=%%I"
)
if defined VS_READY (
    echo [OK] Visual Studio C++ x64 build tools are installed.
    exit /b 0
)
if defined VS_EXISTING (
    if exist "%VSSETUP%" (
        echo.
        echo [INSTALL] Adding Desktop C++ workload to existing Visual Studio:
        echo   %VS_EXISTING%
        rem Modify the existing instance rather than installing a second one.
        "%VSSETUP%" modify --installPath "%VS_EXISTING%" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart
        if errorlevel 1 (
            echo [ERROR] Visual Studio workload modification failed.
            set /a FAILED+=1 >nul
        )
        exit /b 0
    )
)
echo.
echo [INSTALL] Visual Studio 2022 Build Tools + Desktop C++ workload
rem --includeRecommended brings the MSVC toolchain and Windows SDK.
winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --source winget --accept-package-agreements --accept-source-agreements --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
if errorlevel 1 (
    echo [ERROR] Visual Studio Build Tools install failed.
    set /a FAILED+=1 >nul
)
exit /b 0

:InstallPackage
winget install --exact --id "%~1" --source winget --accept-package-agreements --accept-source-agreements --silent --disable-interactivity
if errorlevel 1 (
    echo [ERROR] WinGet could not install %~1.
    set /a FAILED+=1 >nul
)
exit /b 0

:UpgradePackage
winget upgrade --exact --id "%~1" --source winget --accept-package-agreements --accept-source-agreements --silent --disable-interactivity
if errorlevel 1 (
    echo [WARNING] An old or incomplete version was detected, but WinGet could not upgrade %~1.
    echo You may need to update it manually if WinGet does not manage that install.
    set /a FAILED+=1 >nul
)
exit /b 0

:CheckCMake
rem PowerShell checks version numbers without relying on string ordering.
powershell -NoProfile -Command "$v=(& cmake --version 2>$null | Select-Object -First 1); if ($v -match 'cmake version ([0-9]+)\.([0-9]+)') { if ([int]$Matches[1] -gt 3 -or ([int]$Matches[1] -eq 3 -and [int]$Matches[2] -ge 25)) { exit 0 } }; exit 1" >nul 2>&1
exit /b %errorlevel%

:CheckLLVM
where lld-link.exe >nul 2>&1
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$v=(& clang++ --version 2>$null | Select-Object -First 1); if ($v -match 'clang version ([0-9]+)' -and [int]$Matches[1] -ge 20) { exit 0 }; exit 1" >nul 2>&1
exit /b %errorlevel%

:VerifyOnly
echo ============================================================
echo   Fable 2 Recompiled - Requirements Check ONLY
echo ============================================================
echo.
call :Verify
echo.
pause
exit /b 0

:Verify
call :CheckTool "Git" "git.exe"
call :CheckCMake
if errorlevel 1 (echo [CHECK] CMake 3.25+ not found.) else (echo [OK] CMake 3.25+)
call :CheckTool "Ninja" "ninja.exe"
call :CheckLLVM
if errorlevel 1 (echo [CHECK] LLVM Clang 20+ and LLD not found.) else (echo [OK] LLVM Clang 20+ and LLD)
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
if errorlevel 1 (echo [CHECK] Python 3.10+ not found on current PATH.) else (echo [OK] Python 3.10+)
set "HAVE_DOTNET8="
for /f "delims=" %%V in ('dotnet --list-sdks 2^>nul ^| findstr /R /B "8\."') do set "HAVE_DOTNET8=1"
if defined HAVE_DOTNET8 (echo [OK] .NET 8 SDK) else (echo [CHECK] .NET 8 SDK not found on current PATH.)
set "VS_READY="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest`) do set "VS_READY=%%I"
)
if defined VS_READY (echo [OK] Visual Studio C++ tools: %VS_READY%) else (echo [CHECK] Visual Studio C++ workload not detected.)
echo.
echo Note: PATH changes may require a new terminal or a reboot.
exit /b 0

:CheckTool
where "%~2" >nul 2>&1
if errorlevel 1 (echo [CHECK] %~1 not found on current PATH.) else (echo [OK] %~1)
exit /b 0

:Elevate
echo Requesting administrator permission for installation...
set "FABLE2_INSTALLER_PATH=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$q=[char]34; $a='/d /s /c '+$q+$q+$env:FABLE2_INSTALLER_PATH+$q+$q; $p=Start-Process -FilePath $env:ComSpec -ArgumentList $a -Verb RunAs -Wait -PassThru; exit $p.ExitCode"
if errorlevel 1 (
    echo [ERROR] Elevation was cancelled or installation failed.
    pause
    exit /b 1
)
exit /b 0

:Failed
echo.
pause
exit /b 1
