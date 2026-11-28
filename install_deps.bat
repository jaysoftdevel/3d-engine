@echo off
setlocal DisableDelayedExpansion
if "%~1"=="" goto check
if /i "%~1"=="--help" goto help
if /i "%~1"=="--check" goto check
if /i "%~1"=="--install" if "%~2"=="" goto install
echo Usage: install_deps.bat [--check [BUILD OPTIONS] ^| --install] 1>&2
exit /b 2

:help
echo Usage: install_deps.bat [--check [BUILD OPTIONS] ^| --install]
exit /b 0

:check
"%~dp0tools\launch-python.bat" "%~dp0tools\build_windows.py" --root "%~dp0." --check %*
exit /b %errorlevel%

:install
call "%~dp0build.bat" --check >nul 2>&1
if not errorlevel 1 (
    echo Windows build prerequisites are available.
    exit /b 0
)
where.exe winget.exe >nul 2>&1
if errorlevel 1 (
    echo WinGet is unavailable. Install the prerequisites listed in docs\WINDOWS.md. 1>&2
    exit /b 2
)
call "%~dp0tools\launch-python.bat" -c "import sys" >nul 2>&1
if not errorlevel 1 goto install_cpp
winget.exe install --source winget --exact --id Python.Python.3.14
if errorlevel 1 exit /b %errorlevel%

:install_cpp
winget.exe install --source winget --exact --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
if errorlevel 1 exit /b %errorlevel%
echo Open a new Command Prompt and run install_deps.bat --check.
exit /b 0
