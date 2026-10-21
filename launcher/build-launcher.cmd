@echo off
setlocal EnableExtensions
set "ROOT=%~dp0.."
set "PROJECT=%~dp0Fable2.Launcher\Fable2.Launcher.csproj"
set "OUTPUT=%ROOT%\out\game"

rem Internal step called by the root build.cmd; no selectable output or format.
dotnet publish "%PROJECT%" -c Release -r win-x64 --self-contained true ^
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true ^
    -o "%OUTPUT%"
if errorlevel 1 exit /b 1
if not exist "%OUTPUT%\Fable2Launcher.exe" (
    echo Error: dotnet publish did not create Fable2Launcher.exe. 1>&2
    exit /b 1
)
exit /b 0
