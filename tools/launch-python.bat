@echo off
setlocal DisableDelayedExpansion
set "PYTHONDONTWRITEBYTECODE=1"
py.exe -3 -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_py
python.exe -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_python
python3.exe -B -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" >nul 2>&1
if not errorlevel 1 goto use_python3
echo Python 3.10 or newer is required. Install Python with its launcher or add Python to PATH. 1>&2
exit /b 2
:use_py
py.exe -3 -B %*
exit /b %errorlevel%
:use_python
python.exe -B %*
exit /b %errorlevel%
:use_python3
python3.exe -B %*
exit /b %errorlevel%
