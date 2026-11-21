@echo off
setlocal DisableDelayedExpansion
if exist "%~dp0bin\jsd-showcase.exe" goto installed
if exist "%~dp0build\windows-vs2026\bin\Release\jsd-showcase.exe" goto vs2026
if exist "%~dp0build\windows-vs2022\bin\Release\jsd-showcase.exe" goto vs2022
echo Showcase executable not found. Run install.bat first. 1>&2
exit /b 2
:installed
"%~dp0bin\jsd-showcase.exe" %*
exit /b %errorlevel%
:vs2026
"%~dp0build\windows-vs2026\bin\Release\jsd-showcase.exe" %*
exit /b %errorlevel%
:vs2022
"%~dp0build\windows-vs2022\bin\Release\jsd-showcase.exe" %*
exit /b %errorlevel%
