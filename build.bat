@echo off
setlocal DisableDelayedExpansion
set "PYTHONDONTWRITEBYTECODE=1"
py -3 -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_py
python -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_python
python3 -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_python3
echo Python 3.10 or newer is required. Install Python with its launcher or add Python to PATH. 1>&2
exit /b 2
:use_py
py -3 -B "%~dp0tools\build_windows.py" --root "%~dp0."  %*
exit /b %errorlevel%
:use_python
python -B "%~dp0tools\build_windows.py" --root "%~dp0."  %*
exit /b %errorlevel%
:use_python3
python3 -B "%~dp0tools\build_windows.py" --root "%~dp0."  %*
exit /b %errorlevel%
