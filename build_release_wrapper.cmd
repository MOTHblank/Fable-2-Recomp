@echo off
rem Legacy entry point. All Windows builds use the same unified command.
call "%~dp0build.cmd" %*
exit /b %ERRORLEVEL%
