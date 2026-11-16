@echo off
rem One-command build: double-click this, or run "build.cmd" (options: -Install, -Tests, -Clean). See build.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set result=%errorlevel%
rem keep the window open when started by double-clicking
echo %cmdcmdline% | find /i "%~0" >nul && pause
exit /b %result%
